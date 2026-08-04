/* torque_interceptor_sm.c - MAIN MCU (STM32G0B1CBT6)
 *
 * This MCU decides "boost conditions look met" and REQUESTS boost over
 * UART. It cannot flip FORCE_PT/GATE_ENABLE itself - those pins belong
 * to the supervisor. Treat supervisor_approved as ground truth for
 * whether boost is physically active, not this MCU's own state name.
 *
 * HAL/board-specific calls are TODO stubs.
 */
#include "main_mcu.h"
#include <string.h>

/* ---------------------------------------------------------------------
 * Hardware I/O stubs
 * --------------------------------------------------------------------- */
static void hw_send_heartbeat_pulse(void)
{
    /* TODO: HAL_GPIO_TogglePin(HEARTBEAT_OUT_GPIO_Port, HEARTBEAT_OUT_Pin); */
}

static void hw_log_fault(fault_code_t f, const sensor_snapshot_t *snap)
{
    /* TODO: write to flash fault log + UART diagnostics. */
    (void)f;
    (void)snap;
}

static void hw_uart_send(const proto_main_to_super_t *frame)
{
    /* TODO: HAL_UART_Transmit_IT(&huart_supervisor, (uint8_t*)frame, sizeof(*frame)); */
    (void)frame;
}

static bool hw_uart_try_receive(proto_super_to_main_t *frame)
{
    /* TODO: pull from ring buffer, validate sync + checksum before returning true. */
    (void)frame;
    return false;
}

static bool hw_ignition_cycle_detected(void)
{
    /* TODO: wire to your board's ignition/power-cycle sense signal. */
    return false;
}

/* ---------------------------------------------------------------------
 * Individual checks - these already have their own tolerance windows
 * (min/max, sum +/- tolerance). What they did NOT have before is
 * debounce against a single noisy sample - that's handled separately
 * below in input_checks_ok_debounced(), not inside these functions,
 * so each check function stays a pure single-cycle answer.
 * --------------------------------------------------------------------- */
static bool check_power_rail(const sensor_snapshot_t *s)
{
    return (s->rail_5v_mv >= RAIL_5V_MIN_MV) && (s->rail_5v_mv <= RAIL_5V_MAX_MV);
}

static bool check_range(uint16_t adc_val)
{
    return (adc_val >= ADC_MIN_VALID_COUNTS) && (adc_val <= ADC_MAX_VALID_COUNTS);
}

static bool check_correlation(const sensor_snapshot_t *s)
{
    int32_t sum = (int32_t)s->main_adc + (int32_t)s->sub_adc;
    int32_t diff = sum - (int32_t)ADC_EXPECTED_SUM;
    if (diff < 0) diff = -diff;
    return diff <= (int32_t)ADC_SUM_TOLERANCE;
}

static bool check_not_stuck(sm_context_t *ctx, const sensor_snapshot_t *s)
{
    if (s->main_adc != ctx->last_main_adc) {
        ctx->last_main_adc = s->main_adc;
        ctx->last_main_adc_change_ms = s->now_ms;
        return true;
    }
    uint32_t held_ms = s->now_ms - ctx->last_main_adc_change_ms;
    return held_ms < ADC_STUCK_TIMEOUT_MS;
}

static bool check_can_fresh(const sensor_snapshot_t *s)
{
    return s->can_fresh;
}

static bool all_input_checks_pass(sm_context_t *ctx, const sensor_snapshot_t *s, fault_code_t *out_fault)
{
    if (!check_power_rail(s))      { *out_fault = FAULT_POWER_RAIL;      return false; }
    if (!check_range(s->main_adc)) { *out_fault = FAULT_OUT_OF_RANGE;    return false; }
    if (!check_range(s->sub_adc))  { *out_fault = FAULT_OUT_OF_RANGE;    return false; }
    if (!check_correlation(s))     { *out_fault = FAULT_SIGNAL_MISMATCH; return false; }
    if (!check_not_stuck(ctx, s))  { *out_fault = FAULT_SIGNAL_STUCK;    return false; }
    return true;
}

/* Wraps all_input_checks_pass with a consecutive-failure debounce. A
 * single bad cycle is absorbed as noise (returns true, no fault code
 * set) until FAULT_DEBOUNCE_SAMPLES in a row fail, at which point it's
 * treated as real and the fault code is surfaced. This is what makes
 * "single ADC noise = transient, doesn't even need to escalate" true
 * in practice, per the confirmed policy. */
static bool input_checks_ok_debounced(sm_context_t *ctx, const sensor_snapshot_t *s, fault_code_t *out_fault)
{
    fault_code_t raw_fault = FAULT_NONE;
    bool raw_ok = all_input_checks_pass(ctx, s, &raw_fault);

    if (raw_ok) {
        ctx->input_fail_streak = 0;
        return true;
    }

    if (ctx->input_fail_streak < 0xFFu) {
        ctx->input_fail_streak++;
    }

    if (ctx->input_fail_streak < FAULT_DEBOUNCE_SAMPLES) {
        return true; /* still within noise tolerance - hold, don't fault yet */
    }

    *out_fault = raw_fault;
    return false;
}

/* Boost decision with hysteresis: enter above BOOST_THRESHOLD, but once
 * already boosting, only drop out below the lower hysteresis line.
 * Prevents flicker for a can_value sitting right at the edge. */
static bool boost_condition_met(const sensor_snapshot_t *s, bool currently_boosting)
{
    if (!s->can_boost_requested || !check_can_fresh(s)) {
        return false;
    }
    uint16_t effective_threshold = currently_boosting
        ? (uint16_t)(BOOST_THRESHOLD - BOOST_THRESHOLD_HYSTERESIS)
        : BOOST_THRESHOLD;
    return s->can_value > effective_threshold;
}

/* ---------------------------------------------------------------------
 * UART link to supervisor
 * --------------------------------------------------------------------- */
static void uart_send_status(sm_context_t *ctx, const sensor_snapshot_t *s, bool want_boost)
{
    if ((s->now_ms - ctx->last_uart_tx_ms) < PROTO_TX_PERIOD_MS) {
        return;
    }
    proto_main_to_super_t frame;
    frame.sync       = PROTO_FRAME_SYNC;
    frame.seq        = ctx->tx_seq++;
    frame.want_boost = want_boost ? 1u : 0u;
    frame.main_adc   = s->main_adc;
    frame.sub_adc    = s->sub_adc;
    frame.can_value  = s->can_value;
    frame.can_fresh  = s->can_fresh ? 1u : 0u;
    frame.checksum   = proto_checksum((const uint8_t *)&frame, sizeof(frame) - 1u);
    hw_uart_send(&frame);
    ctx->last_uart_tx_ms = s->now_ms;
}

static void uart_poll_receive(sm_context_t *ctx, uint32_t now_ms)
{
    proto_super_to_main_t frame;
    if (hw_uart_try_receive(&frame)) {
        ctx->supervisor_approved = (frame.approved != 0u);
        ctx->supervisor_fault    = (fault_code_t)frame.supervisor_fault;
        ctx->last_valid_rx_ms    = now_ms;
    }
}

static bool uart_link_ok(const sm_context_t *ctx, uint32_t now_ms)
{
    return (now_ms - ctx->last_valid_rx_ms) < PROTO_LINK_TIMEOUT_MS;
}

/* ---------------------------------------------------------------------
 * Fault entry + the confirmed two-bucket recovery policy
 * --------------------------------------------------------------------- */
static void enter_fault(sm_context_t *ctx, fault_code_t f, const sensor_snapshot_t *snap)
{
    ctx->fault = f;
    hw_log_fault(f, snap);
    ctx->state = STATE_FAULT;
    ctx->fault_entered_ms = snap->now_ms;
    ctx->pending_recovery = false;
    ctx->input_fail_streak = 0; /* fresh count once we're actually in FAULT */

    fault_class_t cls = fault_classify(f);
    if (cls == FAULT_CLASS_TRANSIENT) {
        ctx->pending_recovery = true;
        return;
    }

    if (ctx->recovery_latched) {
        return; /* already escalated this drive - stays FAULT until ignition cycle */
    }
    if (f == ctx->last_compute_fault) {
        ctx->recovery_latched = true;
        hw_log_fault(f, snap); /* log the escalation event distinctly */
        return;
    }
    ctx->last_compute_fault = f;
    ctx->pending_recovery = true;
}

static void run_fault_state(sm_context_t *ctx, const sensor_snapshot_t *snap)
{
    if (!ctx->pending_recovery) {
        return;
    }
    if ((snap->now_ms - ctx->fault_entered_ms) < FAULT_RECOVERY_DEBOUNCE_MS) {
        return;
    }
    ctx->pending_recovery = false;
    ctx->state = STATE_STARTUP;
}

void sm_notify_ignition_cycle(sm_context_t *ctx, uint32_t now_ms)
{
    ctx->recovery_latched   = false;
    ctx->last_compute_fault = FAULT_NONE;
    ctx->pending_recovery   = false;
    ctx->input_fail_streak  = 0;
    ctx->state = STATE_STARTUP;
    ctx->fault_entered_ms = now_ms;
}

/* ---------------------------------------------------------------------
 * State machine
 * --------------------------------------------------------------------- */
void sm_init(sm_context_t *ctx, uint32_t now_ms)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->state = STATE_STARTUP;
    ctx->fault = FAULT_NONE;
    ctx->last_heartbeat_sent_ms = now_ms;
    ctx->last_valid_rx_ms = now_ms;
}

void sm_run(sm_context_t *ctx, const sensor_snapshot_t *snap)
{
    if ((snap->now_ms - ctx->last_heartbeat_sent_ms) >= HEARTBEAT_PERIOD_MS) {
        hw_send_heartbeat_pulse();
        ctx->last_heartbeat_sent_ms = snap->now_ms;
    }

    if (hw_ignition_cycle_detected()) {
        sm_notify_ignition_cycle(ctx, snap->now_ms);
    }

    uart_poll_receive(ctx, snap->now_ms);

    fault_code_t new_fault = FAULT_NONE;

    switch (ctx->state) {

    case STATE_STARTUP: {
        uart_send_status(ctx, snap, false);
        /* Self-test intentionally uses the RAW (non-debounced) check -
         * this is the one place a bad sample should NOT be forgiven,
         * since the point is confirming the signal is trustworthy
         * before entering service at all. */
        if (all_input_checks_pass(ctx, snap, &new_fault)) {
            ctx->state = STATE_NORMAL_PASSTHROUGH;
        } else {
            enter_fault(ctx, FAULT_SELFTEST_FAILED, snap);
        }
        break;
    }

    case STATE_NORMAL_PASSTHROUGH: {
        if (!input_checks_ok_debounced(ctx, snap, &new_fault)) {
            enter_fault(ctx, new_fault, snap);
            break;
        }
        if (!uart_link_ok(ctx, snap->now_ms)) {
            enter_fault(ctx, FAULT_LINK_TIMEOUT, snap);
            break;
        }

        bool want_boost = boost_condition_met(snap, false);
        uart_send_status(ctx, snap, want_boost);

        if (want_boost && ctx->supervisor_approved) {
            ctx->state = STATE_BOOST_ACTIVE;
        }
        break;
    }

    case STATE_BOOST_ACTIVE: {
        if (!input_checks_ok_debounced(ctx, snap, &new_fault)) {
            enter_fault(ctx, new_fault, snap);
            break;
        }
        if (!uart_link_ok(ctx, snap->now_ms)) {
            enter_fault(ctx, FAULT_LINK_TIMEOUT, snap);
            break;
        }

        bool still_wants_boost = boost_condition_met(snap, true);
        uart_send_status(ctx, snap, still_wants_boost);

        if (!ctx->supervisor_approved) {
            fault_code_t f = (ctx->supervisor_fault != FAULT_NONE)
                                ? ctx->supervisor_fault
                                : FAULT_SUPERVISOR_DENIED;
            enter_fault(ctx, f, snap);
            break;
        }
        if (!still_wants_boost) {
            ctx->state = STATE_NORMAL_PASSTHROUGH;
            break;
        }

        /* TODO: boost AMOUNT is not yet determined. Compute dac_main/
         * dac_sub from can_value / main_adc here once that formula is
         * decided, then send via whatever DAC-write path you use -
         * this file only handles the decision/request layer. */
        break;
    }

    case STATE_FAULT: {
        uart_send_status(ctx, snap, false);
        run_fault_state(ctx, snap);
        break;
    }

    default:
        ctx->state = STATE_FAULT;
        ctx->fault = FAULT_SELFTEST_FAILED;
        break;
    }
}

const char *sm_state_name(sm_state_t s)
{
    switch (s) {
    case STATE_STARTUP:            return "STARTUP";
    case STATE_NORMAL_PASSTHROUGH: return "NORMAL_PASSTHROUGH";
    case STATE_BOOST_ACTIVE:       return "BOOST_ACTIVE";
    case STATE_FAULT:               return "FAULT";
    default:                        return "UNKNOWN";
    }
}
