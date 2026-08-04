/* supervisor_fw.c - SAFETY SUPERVISOR (STM32G030F6P6TR)
 *
 * Independent veto authority. Approves boost only if ALL of:
 *   - its own MAIN_ADC/SUB_ADC checks pass (debounced against noise)
 *   - the FAULT_OUT input from the hardware output monitor is NOT asserted
 *   - main MCU's heartbeat pin is toggling within timeout
 *   - the UART link from the main MCU is alive and well-formed
 *   - the main MCU's latest request says want_boost = true
 *
 * If ANY of those fail, defaults to deny + drives FORCE_PT/GATE_ENABLE
 * low (pass-through), regardless of what the main MCU asks.
 *
 * CORRECTED from the previous draft: FAULT_OUT is READ here, not driven.
 * The LM393 output-monitor comparator generates it entirely in hardware
 * and this chip just consumes it as one more gating input - confirmed
 * by signal_flow_all_scenarios.html's fault scenario.
 */
#include "main_supervisor.h"
#include <string.h>
#include "reg_defs.h"   /* our own minimal register definitions - see that file's header comment */

/* ---------------------------------------------------------------------
 * Hardware I/O
 * --------------------------------------------------------------------- */

/* FORCE_PT = PA6, GATE_ENABLE = PA5 (confirmed against schematic).
 * Call this ONCE at boot, before sup_init() runs - sup_init() calls
 * hw_set_gate(false) as an immediate safety default, but that write is
 * meaningless until GPIOA's clock is enabled and both pins are
 * actually configured as outputs. */
static void hw_gate_pins_init(void)
{
    /* 1. Enable GPIOA clock - without this, GPIOA registers don't respond at all */
    RCC->IOPENR |= RCC_IOPENR_GPIOAEN;

    /* 2. Set PA5 and PA6 to OUTPUT mode.
     * MODER uses 2 bits per pin: 00=input, 01=output, 10=alt func, 11=analog
     * Clear both bit-pairs first, then set to 01 (output) */
    GPIOA->MODER &= ~((3U << (5 * 2)) | (3U << (6 * 2)));  /* clear PA5, PA6 mode bits */
    GPIOA->MODER |=  ((1U << (5 * 2)) | (1U << (6 * 2)));  /* set both to 01 = output */

    /* 3. Push-pull, not open-drain (OTYPER bit = 0 means push-pull, which is
     * default after reset, but set explicitly so it's not left to chance) */
    GPIOA->OTYPER &= ~((1U << 5) | (1U << 6));

    /* 4. Safe default: both pins LOW at boot = pass-through, before any
     * decision logic has even run yet */
    GPIOA->BSRR = (1U << (5 + 16)) | (1U << (6 + 16));
}

static void hw_set_gate(bool enable)
{
    /* Polarity per schematic: FORCE_PT and GATE_ENABLE both HIGH ->
     * AND_OUT high -> analog switch NO path -> DAC/boost live.
     * Either LOW -> AND_OUT low -> NC -> OEM pass-through (safe default). */
    if (enable) {
        GPIOA->BSRR = (1U << 6) | (1U << 5);               /* PA6, PA5 HIGH */
    } else {
        GPIOA->BSRR = (1U << (6 + 16)) | (1U << (5 + 16));  /* PA6, PA5 LOW */
    }
}

static void hw_log_fault(fault_code_t f)
{
    /* TODO: own diagnostic log / UART_TX on this chip, if wired to one. */
    (void)f;
}

static void hw_uart_send(const proto_super_to_main_t *frame)
{
    /* TODO: HAL_UART_Transmit_IT(&huart_main, (uint8_t*)frame, sizeof(*frame)); */
    (void)frame;
}

static bool hw_uart_try_receive(proto_main_to_super_t *frame)
{
    /* TODO: pull from ring buffer, validate sync + checksum before returning true. */
    (void)frame;
    return false;
}

/* ---------------------------------------------------------------------
 * Independent checks - own input-side (MAIN_ADC/SUB_ADC) validity only.
 * Output-side validity (MAIN_OUT/SUB_OUT vs VREF) is handled entirely
 * by the LM393 hardware comparator, consumed below via fault_out_asserted -
 * there is deliberately no software re-check of the output side here.
 * --------------------------------------------------------------------- */
static bool check_range(uint16_t adc_val)
{
    return (adc_val >= SUP_ADC_MIN_VALID_COUNTS) && (adc_val <= SUP_ADC_MAX_VALID_COUNTS);
}

static bool check_correlation(const sup_snapshot_t *s)
{
    int32_t sum = (int32_t)s->main_adc + (int32_t)s->sub_adc;
    int32_t diff = sum - (int32_t)SUP_ADC_EXPECTED_SUM;
    if (diff < 0) diff = -diff;
    return diff <= (int32_t)SUP_ADC_SUM_TOLERANCE;
}

/* GAP FIX: supervisor previously never checked its own 5V rail at all,
 * even though it's the chip with final veto authority over boost. */
static bool check_power_rail(const sup_snapshot_t *s)
{
    return (s->rail_5v_mv >= SUP_RAIL_5V_MIN_MV) && (s->rail_5v_mv <= SUP_RAIL_5V_MAX_MV);
}

static bool own_checks_pass(const sup_snapshot_t *s, fault_code_t *out_fault)
{
    if (!check_power_rail(s))       { *out_fault = FAULT_POWER_RAIL;     return false; }
    if (!check_range(s->main_adc)) { *out_fault = FAULT_OUT_OF_RANGE;    return false; }
    if (!check_range(s->sub_adc))  { *out_fault = FAULT_OUT_OF_RANGE;    return false; }
    if (!check_correlation(s))      { *out_fault = FAULT_SIGNAL_MISMATCH; return false; }
    return true;
}

/* Debounced wrapper, same pattern/intent as the main MCU's - absorbs a
 * single noisy sample rather than immediately declaring a fault.
 *
 * FIX: same bug as the main MCU had - streak now resets when the raw
 * fault type CHANGES between cycles, so a rail glitch followed by a
 * range glitch followed by a correlation glitch (three different
 * one-off problems) no longer gets mistaken for one problem persisting
 * 3 cycles. */
static bool own_checks_ok_debounced(sup_context_t *ctx, const sup_snapshot_t *s, fault_code_t *out_fault)
{
    fault_code_t raw_fault = FAULT_NONE;
    bool raw_ok = own_checks_pass(s, &raw_fault);

    if (raw_ok) {
        ctx->input_fail_streak = 0;
        ctx->input_fail_last_code = FAULT_NONE;
        return true;
    }

    if (raw_fault != ctx->input_fail_last_code) {
        ctx->input_fail_last_code = raw_fault;
        ctx->input_fail_streak = 1;
    } else if (ctx->input_fail_streak < 0xFFu) {
        ctx->input_fail_streak++;
    }

    if (ctx->input_fail_streak < SUP_FAULT_DEBOUNCE_SAMPLES) {
        return true;
    }
    *out_fault = raw_fault;
    return false;
}

/* ---------------------------------------------------------------------
 * Heartbeat monitor - watches the physical pin directly, independent
 * of the UART link.
 * --------------------------------------------------------------------- */
static bool heartbeat_ok(sup_context_t *ctx, const sup_snapshot_t *s)
{
    if (s->heartbeat_pin_state != ctx->last_heartbeat_pin_state) {
        ctx->last_heartbeat_pin_state = s->heartbeat_pin_state;
        ctx->last_heartbeat_change_ms = s->now_ms;
        return true;
    }
    uint32_t held_ms = s->now_ms - ctx->last_heartbeat_change_ms;
    return held_ms < SUP_HEARTBEAT_TIMEOUT_MS;
}

/* ---------------------------------------------------------------------
 * UART handling
 * --------------------------------------------------------------------- */
static void uart_poll_receive(sup_context_t *ctx, uint32_t now_ms)
{
    proto_main_to_super_t frame;
    if (hw_uart_try_receive(&frame)) {
        ctx->last_rx_seq        = frame.seq;
        ctx->req_want_boost     = (frame.want_boost != 0u);
        ctx->have_valid_request = true;
        ctx->last_valid_rx_ms   = now_ms;
    }
}

static bool link_ok(const sup_context_t *ctx, uint32_t now_ms)
{
    return ctx->have_valid_request &&
           (now_ms - ctx->last_valid_rx_ms) < PROTO_LINK_TIMEOUT_MS;
}

static void uart_send_response(sup_context_t *ctx, bool approved)
{
    proto_super_to_main_t frame;
    frame.sync             = PROTO_FRAME_SYNC;
    frame.seq_echo         = ctx->last_rx_seq;
    frame.approved         = approved ? 1u : 0u;
    frame.supervisor_fault = (uint8_t)ctx->current_fault;
    frame.checksum         = proto_checksum((const uint8_t *)&frame, sizeof(frame) - 1u);
    hw_uart_send(&frame);
}

/* ---------------------------------------------------------------------
 * Main entry point
 * --------------------------------------------------------------------- */
void sup_init(sup_context_t *ctx, uint32_t now_ms)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->last_heartbeat_change_ms = now_ms;
    ctx->current_fault = FAULT_NONE;
    hw_gate_pins_init();  /* clock enable + mode config, must happen first */
    hw_set_gate(false);   /* default safe state before anything else runs */
}

void sup_run(sup_context_t *ctx, const sup_snapshot_t *snap)
{
    uart_poll_receive(ctx, snap->now_ms);

    fault_code_t local_fault = FAULT_NONE;
    bool checks_ok = own_checks_ok_debounced(ctx, snap, &local_fault);
    bool hb_ok     = heartbeat_ok(ctx, snap);
    bool uart_ok   = link_ok(ctx, snap->now_ms);
    bool output_ok = !snap->fault_out_asserted; /* hardware comparator's own verdict, read directly */

    fault_code_t reported_fault = FAULT_NONE;
    if (!checks_ok)      reported_fault = local_fault;
    else if (!output_ok) reported_fault = FAULT_OUTPUT_MISMATCH;
    else if (!hb_ok)     reported_fault = FAULT_HEARTBEAT_LOST;
    else if (!uart_ok)   reported_fault = FAULT_LINK_TIMEOUT;

    if (reported_fault != ctx->current_fault) {
        hw_log_fault(reported_fault);
    }
    ctx->current_fault = reported_fault;

    bool approve = checks_ok && output_ok && hb_ok && uart_ok && ctx->req_want_boost;

    hw_set_gate(approve);
    ctx->gate_state = approve;

    uart_send_response(ctx, approve);
}
