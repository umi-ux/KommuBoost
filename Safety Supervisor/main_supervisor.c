/* main_supervisor.c - SAFETY SUPERVISOR (STM32G030F6P6TR)
 *
 * INTEGRATION NOTES for main.c:
 *   1. Add near the top, after other includes:
 *        #include "main_supervisor.h"
 *   2. In main(), inside "USER CODE BEGIN 2":
 *        sup_context_t ctx;
 *        sup_init(&ctx, sup_now_ms());
 *   3. Inside the while(1) loop, "USER CODE BEGIN WHILE":
 *        if (sup_loop_ready()) {
 *            sup_snapshot_t snap;
 *            sup_read_snapshot(&snap);
 *            sup_run(&ctx, &snap);
 *        }
 *   4. No changes needed to stm32g0xx_it.c - HAL_UART_RxCpltCallback is
 *      defined in THIS file (weak-function override).
 *
 * Watchdog (IWDG) is wired in - hiwdg confirmed present in this
 * project's main.c (prescaler 32, reload 500, same as main MCU).
 * Refreshed every sup_run() cycle; a watchdog-caused reset is detected
 * at init and reported once via current_fault/UART.
 */
#include "main_supervisor.h"
#include "main.h"
#include <string.h>

/* Basic CubeMX application structure - these handles are plain globals
 * in main.c with no extern anywhere else, same situation as main MCU. */
extern ADC_HandleTypeDef  hadc1;
extern UART_HandleTypeDef huart2;
extern IWDG_HandleTypeDef hiwdg;

static void hw_watchdog_refresh(void)
{
    HAL_IWDG_Refresh(&hiwdg);
}

/* ---------------------------------------------------------------------
 * ADC - channel-switching read, since MX_ADC1_Init() only configured
 * ADC_CHANNEL_0 (MAIN_ADC/PA0), same situation as the main MCU.
 * --------------------------------------------------------------------- */
static bool hw_adc_read(uint32_t channel, uint16_t *out_value)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;

    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        return false;
    }
    if (HAL_ADC_Start(&hadc1) != HAL_OK) {
        return false;
    }
    if (HAL_ADC_PollForConversion(&hadc1, 10u) != HAL_OK) {
        HAL_ADC_Stop(&hadc1);
        return false;
    }
    *out_value = (uint16_t)HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return true;
}

/* ---------------------------------------------------------------------
 * GPIO - HEARTBEAT (input, watches main MCU's PA12), FAULT_OUT (input,
 * from the LM393 comparator), SUPERVISOR_GATE_ENABLE (output, this
 * chip's own half of the AND-gate decision).
 * --------------------------------------------------------------------- */
static bool hw_read_heartbeat_pin(void)
{
    return HAL_GPIO_ReadPin(HEARTBEAT_GPIO_Port, HEARTBEAT_Pin) == GPIO_PIN_SET;
}

static bool hw_read_fault_out_pin(void)
{
    return HAL_GPIO_ReadPin(FAULT_OUT_GPIO_Port, FAULT_OUT_Pin) == GPIO_PIN_SET;
}

static void hw_set_supervisor_gate(bool enable)
{
    HAL_GPIO_WritePin(SUPERVISOR_GATE_ENABLE_GPIO_Port, SUPERVISOR_GATE_ENABLE_Pin,
                       enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void hw_log_fault(fault_code_t f)
{
    /* TODO: diagnostic log, if/when needed. */
    (void)f;
}

/* ---------------------------------------------------------------------
 * UART - diagnostic/secondary channel only, see header note. Same
 * non-blocking pattern as the main MCU (9600 baud, ~8ms per 8-byte
 * frame, too slow to block a 10ms loop on).
 * --------------------------------------------------------------------- */
static volatile bool s_rx_pending = false;
static proto_main_to_super_t s_rx_buf;

static void hw_uart_rx_arm(void)
{
    HAL_UART_Receive_IT(&huart2, (uint8_t *)&s_rx_buf, sizeof(s_rx_buf));
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        s_rx_pending = true;
        /* Do NOT re-arm here - main loop copies out first, then re-arms. */
    }
}

static void hw_uart_send(const proto_super_to_main_t *frame)
{
    HAL_UART_Transmit_IT(&huart2, (const uint8_t *)frame, sizeof(*frame));
}

static bool hw_uart_try_receive(proto_main_to_super_t *out_frame)
{
    if (!s_rx_pending) {
        return false;
    }
    proto_main_to_super_t local;
    memcpy(&local, (const void *)&s_rx_buf, sizeof(local));
    s_rx_pending = false;
    hw_uart_rx_arm();

    if (local.sync != PROTO_FRAME_SYNC) {
        return false;
    }
    uint8_t calc = proto_checksum((const uint8_t *)&local, sizeof(local) - 1u);
    if (calc != local.checksum) {
        return false;
    }
    *out_frame = local;
    return true;
}

/* ---------------------------------------------------------------------
 * Independent checks - own input-side (MAIN_ADC/SUB_ADC) validity
 * only. Output-side validity is handled entirely by the LM393 hardware
 * comparator, consumed via fault_out_asserted.
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

static bool own_checks_pass(const sup_snapshot_t *s, fault_code_t *out_fault)
{
    if (!check_range(s->main_adc)) { *out_fault = FAULT_OUT_OF_RANGE;    return false; }
    if (!check_range(s->sub_adc))  { *out_fault = FAULT_OUT_OF_RANGE;    return false; }
    if (!check_correlation(s))      { *out_fault = FAULT_SIGNAL_MISMATCH; return false; }
    return true;
}

/* Debounce - same per-fault-type fix as the main MCU. */
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

static bool heartbeat_ok(sup_context_t *ctx, const sup_snapshot_t *s)
{
    if (s->heartbeat_pin_state != ctx->last_heartbeat_pin_state) {
        ctx->last_heartbeat_pin_state = s->heartbeat_pin_state;
        ctx->last_heartbeat_change_ms = s->now_ms;
        return true;
    }
    return (s->now_ms - ctx->last_heartbeat_change_ms) < SUP_HEARTBEAT_TIMEOUT_MS;
}

/* ---------------------------------------------------------------------
 * UART polling - diagnostic only, does not affect the gate decision.
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

static void uart_send_response(sup_context_t *ctx, const sup_snapshot_t *snap, bool approved)
{
    if ((snap->now_ms - ctx->last_uart_tx_ms) < PROTO_TX_PERIOD_MS) {
        return;
    }
    proto_super_to_main_t frame;
    frame.sync             = PROTO_FRAME_SYNC;
    frame.seq_echo         = ctx->last_rx_seq;
    frame.approved          = approved ? 1u : 0u;
    frame.supervisor_fault = (uint8_t)ctx->current_fault;
    frame.checksum         = proto_checksum((const uint8_t *)&frame, sizeof(frame) - 1u);
    hw_uart_send(&frame);
    ctx->last_uart_tx_ms = snap->now_ms;
}

/* ---------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */
void sup_init(sup_context_t *ctx, uint32_t now_ms)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->last_heartbeat_change_ms = now_ms;
    ctx->current_fault = FAULT_NONE;

    /* Check BEFORE clearing: was this boot caused by the watchdog
     * timing out? Reported once on the first sup_run() cycle. */
    ctx->pending_watchdog_fault = __HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != 0;
    __HAL_RCC_CLEAR_RESET_FLAGS();

    hw_uart_rx_arm();
    hw_set_supervisor_gate(false); /* safe default - GPIO init in main.c
                                     * already does this too, redundant
                                     * but harmless belt-and-suspenders */
}

bool sup_loop_ready(void)
{
    static uint32_t last_run_ms = 0;
    uint32_t now = HAL_GetTick();
    if ((now - last_run_ms) < SUP_LOOP_PERIOD_MS) {
        return false;
    }
    last_run_ms = now;
    return true;
}

uint32_t sup_now_ms(void)
{
    return HAL_GetTick();
}

void sup_read_snapshot(sup_snapshot_t *snap)
{
    snap->now_ms = HAL_GetTick();

    uint16_t main_adc = 0, sub_adc = 0;
    hw_adc_read(ADC_CHANNEL_0, &main_adc); /* MAIN_ADC / PA0 */
    hw_adc_read(ADC_CHANNEL_1, &sub_adc);  /* SUB_ADC  / PA1 */
    snap->main_adc = main_adc;
    snap->sub_adc  = sub_adc;

    snap->heartbeat_pin_state = hw_read_heartbeat_pin();
    snap->fault_out_asserted  = hw_read_fault_out_pin();
}

void sup_run(sup_context_t *ctx, const sup_snapshot_t *snap)
{
    hw_watchdog_refresh(); /* must happen every cycle - if this call stops
                             * happening (hung loop), the chip auto-resets */

    uart_poll_receive(ctx, snap->now_ms); /* diagnostic only */

    fault_code_t local_fault = FAULT_NONE;
    bool checks_ok = own_checks_ok_debounced(ctx, snap, &local_fault);
    bool hb_ok     = heartbeat_ok(ctx, snap);
    bool output_ok = !snap->fault_out_asserted;

    fault_code_t reported_fault = FAULT_NONE;
    if (ctx->pending_watchdog_fault) {
        /* Report once, for diagnostic visibility, even if this cycle's
         * live checks all pass - a watchdog reset happening at all is
         * worth surfacing. Unlike the main MCU, this chip has no
         * fault-recovery state machine to route through (continuously
         * re-evaluated by design, see header) - so this doesn't force
         * the gate low beyond whatever the live checks below already
         * decide; it's purely a one-time log/UART report. */
        reported_fault = FAULT_WATCHDOG_RESET;
        ctx->pending_watchdog_fault = false;
    } else if (!checks_ok) {
        reported_fault = local_fault;
    } else if (!output_ok) {
        reported_fault = FAULT_OUTPUT_MISMATCH;
    } else if (!hb_ok) {
        reported_fault = FAULT_HEARTBEAT_LOST;
    }

    if (reported_fault != ctx->current_fault) {
        hw_log_fault(reported_fault);
    }
    ctx->current_fault = reported_fault;

    /* This chip's OWN verdict, from ONLY its own readings - does not
     * depend on anything received over UART. The main MCU independently
     * drives its own gate pin from its own reasoning; the AND gate
     * downstream is what combines the two. */
    bool approve = checks_ok && output_ok && hb_ok;

    hw_set_supervisor_gate(approve);
    ctx->gate_state = approve;

    uart_send_response(ctx, snap, approve); /* diagnostic - informs main
                                              * MCU what this chip sees,
                                              * doesn't gate anything */
}
