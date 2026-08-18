/* main_mcu.c - MAIN MCU (STM32G0B1CBT6)
 *
 * Uses the real CubeMX-generated handles (hadc1, hdac1, hfdcan1, huart1)
 * and pin macros (HEARTBEAT_Pin, MCU_GATE_ENABLE_Pin, etc, from main.h)
 * from the user's actual generated project. No custom register code,
 * no custom SysTick - HAL already owns SysTick via HAL_GetTick().
 *
 * INTEGRATION NOTES for main.c:
 *   1. Add near the top, after other includes:
 *        #include "main_mcu.h"
 *   2. In main(), inside "USER CODE BEGIN 2" (after MX_*_Init() calls,
 *      before the while loop):
 *        sm_context_t ctx;
 *        sm_init(&ctx, sm_now_ms());
 *   3. Inside the while(1) loop, "USER CODE BEGIN WHILE":
 *        if (sm_loop_ready()) {
 *            sensor_snapshot_t snap;
 *            sm_read_snapshot(&snap);
 *            sm_run(&ctx, &snap);
 *        }
 *   4. No changes needed to stm32g0xx_it.c - HAL_UART_RxCpltCallback
 *      is defined in THIS file (weak-function override), and USART1's
 *      interrupt handler is already wired to call it via the HAL/CMSIS
 *      startup code CubeMX generated.
 */
#include "main_mcu.h"
#include "main.h"
#include <string.h>

/* Basic CubeMX application structure declares these handles directly
 * inside main.c as plain globals, with no extern anywhere (unlike
 * Advanced structure, which auto-generates adc.h/dac.h/etc with
 * extern declarations for you). Declaring them here ourselves is what
 * makes them visible to this file - names must match main.c exactly. */
extern ADC_HandleTypeDef   hadc1;
extern DAC_HandleTypeDef   hdac1;
extern FDCAN_HandleTypeDef hfdcan1;
extern UART_HandleTypeDef  huart1;
extern IWDG_HandleTypeDef  hiwdg;

/* ---------------------------------------------------------------------
 * ADC - channel-switching read, since MX_ADC1_Init() only configured
 * ADC_CHANNEL_0 (MAIN_ADC/PA0). SUB_ADC (PA1/ADC_CHANNEL_1) is read by
 * reconfiguring the channel each time - simpler than adding scan mode
 * in CubeMX, fine at our slow (10ms) loop rate.
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
 * DAC - both channels started once at init, value updated each cycle.
 * --------------------------------------------------------------------- */
static void hw_dac_init(void)
{
    HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
    HAL_DAC_Start(&hdac1, DAC_CHANNEL_2);
}

static void hw_dac_write(uint16_t main_val, uint16_t sub_val)
{
    HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, main_val);
    HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, sub_val);
}

/* ---------------------------------------------------------------------
 * FDCAN - filter for the candidate STEERING_TORQUE message (ID 107,
 * *** UNCONFIRMED, see main_mcu.h header ***), classic frame, polled
 * RX FIFO each cycle (simpler than interrupt-driven for this rate).
 * --------------------------------------------------------------------- */
static uint32_t s_last_can_rx_ms = 0;

static void hw_can_init(void)
{
    FDCAN_FilterTypeDef sFilterConfig = {0};
    sFilterConfig.IdType = FDCAN_STANDARD_ID;
    sFilterConfig.FilterIndex = 0;
    sFilterConfig.FilterType = FDCAN_FILTER_MASK;
    sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    sFilterConfig.FilterID1 = CAN_ID_STEERING_LKAS;
    sFilterConfig.FilterID2 = 0x7FFu; /* exact-match mask */
    HAL_FDCAN_ConfigFilter(&hfdcan1, &sFilterConfig);

    HAL_FDCAN_ConfigGlobalFilter(&hfdcan1, FDCAN_REJECT, FDCAN_REJECT,
                                  FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE);

    HAL_FDCAN_Start(&hfdcan1);
}

/* Decodes BO_ 464 STEERING_LKAS. Returns true if a fresh frame with
 * this ID was received this call.
 *
 * Bit extraction derivation (motorola/big-endian DBC convention):
 * for a signal with DBC start bit s, byte_index = s/8, local_bit = s%8
 * (bit7=MSB..bit0=LSB within that byte). Multi-bit signals continue
 * from the start bit toward the LSB, then continue at the MSB of the
 * next byte. Cross-checked self-consistently against the earlier
 * ID 107 message's whole-byte signals (start bit 15 -> byte[1] whole,
 * start bit 31 -> byte[3] whole) before being trusted here - but still
 * VERIFY against a real captured frame with a known STEER_CMD value.
 *
 *   STEER_CMD (start=7, len=11, signed but treated unsigned - see
 *   header comment): byte[0] (all 8 bits, MSB-first) forms the upper
 *   8 bits, byte[1]'s top 3 bits (bits 7,6,5) form the lower 3 bits.
 *   raw = (byte[0] << 3) | (byte[1] >> 5)
 *
 *   STEER_REQ (start=21, len=1): byte_index = 21/8 = 2, local_bit =
 *   21%8 = 5. raw = (byte[2] >> 5) & 0x1
 */
static bool hw_can_try_receive(uint16_t *out_steer_cmd, bool *out_steer_req, uint32_t now_ms)
{
    if (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0) == 0) {
        return false;
    }
    FDCAN_RxHeaderTypeDef RxHeader;
    uint8_t RxData[8] = {0};
    if (HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &RxHeader, RxData) != HAL_OK) {
        return false;
    }
    if (RxHeader.Identifier != CAN_ID_STEERING_LKAS) {
        return false; /* shouldn't happen given the filter, but check anyway */
    }

    uint16_t raw_cmd = ((uint16_t)RxData[0] << 3) | ((uint16_t)RxData[1] >> 5);
    *out_steer_cmd = raw_cmd & 0x07FFu; /* mask to 11 bits, defensive */
    *out_steer_req = ((RxData[2] >> 5) & 0x01u) != 0u;

    s_last_can_rx_ms = now_ms;
    return true;
}

/* ---------------------------------------------------------------------
 * UART - interrupt-driven, NOT blocking. At 9600 baud an 8-byte frame
 * takes ~8.3ms to transmit - blocking would eat nearly our entire
 * 10ms loop period. HAL_UART_Transmit_IT/Receive_IT hand the actual
 * shifting-out to hardware+ISR, letting sm_run() return immediately.
 * --------------------------------------------------------------------- */
static volatile bool s_rx_pending = false;
static proto_super_to_main_t s_rx_buf;

static void hw_uart_rx_arm(void)
{
    HAL_UART_Receive_IT(&huart1, (uint8_t *)&s_rx_buf, sizeof(s_rx_buf));
}

/* HAL weak-function override - HAL's own USART1 IRQ handler (already
 * wired by CubeMX-generated startup code) calls this automatically
 * when a full frame arrives. No changes needed elsewhere. */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        s_rx_pending = true;
        /* Do NOT re-arm here - main loop copies s_rx_buf out first,
         * then re-arms, to avoid a race where a second frame overwrites
         * s_rx_buf before it's been read. */
    }
}

static void hw_uart_send(const proto_main_to_super_t *frame)
{
    /* Non-blocking - HAL copies the buffer into its own TX state and
     * returns immediately; actual byte shifting happens via interrupt. */
    HAL_UART_Transmit_IT(&huart1, (const uint8_t *)frame, sizeof(*frame));
}

static bool hw_uart_try_receive(proto_super_to_main_t *out_frame)
{
    if (!s_rx_pending) {
        return false;
    }
    proto_super_to_main_t local;
    memcpy(&local, (const void *)&s_rx_buf, sizeof(local));
    s_rx_pending = false;
    hw_uart_rx_arm(); /* re-arm for the next frame before validating this one */

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
 * Heartbeat + gate pins - plain GPIO, already configured as outputs by
 * MX_GPIO_Init() in the generated main.c.
 * --------------------------------------------------------------------- */
static void hw_send_heartbeat_pulse(void)
{
    HAL_GPIO_TogglePin(HEARTBEAT_GPIO_Port, HEARTBEAT_Pin);
}

static void hw_set_mcu_gate(bool enable)
{
    HAL_GPIO_WritePin(MCU_GATE_ENABLE_GPIO_Port, MCU_GATE_ENABLE_Pin,
                       enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ---------------------------------------------------------------------
 * Watchdog - IWDG must be enabled in CubeMX (System Core -> IWDG ->
 * Activated) for hiwdg to exist. If sm_run() ever stops being called
 * (hung loop, crash), nothing refreshes the watchdog and the chip
 * auto-resets - caught afterward as FAULT_WATCHDOG_RESET via the
 * normal reset-cause check pattern (not yet wired here - TODO).
 * --------------------------------------------------------------------- */
static void hw_watchdog_refresh(void)
{
    HAL_IWDG_Refresh(&hiwdg);
}

static void hw_log_fault(fault_code_t f, const sensor_snapshot_t *snap)
{
    /* TODO: flash log / diagnostic output, if/when needed. */
    (void)f;
    (void)snap;
}

static bool hw_ignition_cycle_detected(void)
{
    /* TODO: wire to an actual ignition/power-cycle sense signal once
     * that's defined on the schematic. */
    return false;
}

/* ---------------------------------------------------------------------
 * Individual input checks
 * --------------------------------------------------------------------- */
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

/* Watches BOTH MAIN_ADC and SUB_ADC for a frozen reading. */
static bool check_not_stuck(sm_context_t *ctx, const sensor_snapshot_t *s)
{
    bool main_ok, sub_ok;

    if (s->main_adc != ctx->last_main_adc) {
        ctx->last_main_adc = s->main_adc;
        ctx->last_main_adc_change_ms = s->now_ms;
        main_ok = true;
    } else {
        main_ok = (s->now_ms - ctx->last_main_adc_change_ms) < ADC_STUCK_TIMEOUT_MS;
    }

    if (s->sub_adc != ctx->last_sub_adc) {
        ctx->last_sub_adc = s->sub_adc;
        ctx->last_sub_adc_change_ms = s->now_ms;
        sub_ok = true;
    } else {
        sub_ok = (s->now_ms - ctx->last_sub_adc_change_ms) < ADC_STUCK_TIMEOUT_MS;
    }

    return main_ok && sub_ok;
}

static bool check_can_fresh(const sensor_snapshot_t *s)
{
    return s->can_fresh;
}

static bool all_input_checks_pass(sm_context_t *ctx, const sensor_snapshot_t *s, fault_code_t *out_fault)
{
    if (!check_range(s->main_adc)) { *out_fault = FAULT_OUT_OF_RANGE;    return false; }
    if (!check_range(s->sub_adc))  { *out_fault = FAULT_OUT_OF_RANGE;    return false; }
    if (!check_correlation(s))     { *out_fault = FAULT_SIGNAL_MISMATCH; return false; }
    if (!check_not_stuck(ctx, s))  { *out_fault = FAULT_SIGNAL_STUCK;    return false; }
    return true;
}

/* Debounce - streak tracks WHICH fault type is repeating, so unrelated
 * one-off glitches of different types don't get mistaken for one
 * problem persisting. */
static bool input_checks_ok_debounced(sm_context_t *ctx, const sensor_snapshot_t *s, fault_code_t *out_fault)
{
    fault_code_t raw_fault = FAULT_NONE;
    bool raw_ok = all_input_checks_pass(ctx, s, &raw_fault);

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

    if (ctx->input_fail_streak < FAULT_DEBOUNCE_SAMPLES) {
        return true;
    }
    *out_fault = raw_fault;
    return false;
}

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
 * UART link (diagnostic channel - see shared_protocol.h header note)
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
        ctx->supervisor_approved = (frame.approved != 0u); /* informational */
        ctx->supervisor_fault    = (fault_code_t)frame.supervisor_fault;
        ctx->last_valid_rx_ms    = now_ms;
    }
}

static bool uart_link_ok(const sm_context_t *ctx, uint32_t now_ms)
{
    return (now_ms - ctx->last_valid_rx_ms) < PROTO_LINK_TIMEOUT_MS;
}

/* ---------------------------------------------------------------------
 * Fault entry - always forces the gate pin low immediately, regardless
 * of recovery bucket, since that's the safety-critical action.
 * --------------------------------------------------------------------- */
static void enter_fault(sm_context_t *ctx, fault_code_t f, const sensor_snapshot_t *snap)
{
    hw_set_mcu_gate(false);
    ctx->mcu_gate_state = false;

    ctx->fault = f;
    hw_log_fault(f, snap);
    ctx->state = STATE_FAULT;
    ctx->fault_entered_ms = snap->now_ms;
    ctx->pending_recovery = false;
    ctx->input_fail_streak = 0;
    ctx->input_fail_last_code = FAULT_NONE;

    fault_class_t cls = fault_classify(f);
    if (cls == FAULT_CLASS_TRANSIENT) {
        ctx->pending_recovery = true;
        return;
    }

    if (ctx->recovery_latched) {
        return;
    }

    uint32_t bit = (1u << (uint32_t)f);
    if (ctx->compute_fault_seen_mask & bit) {
        ctx->recovery_latched = true;
        hw_log_fault(f, snap);
        return;
    }
    ctx->compute_fault_seen_mask |= bit;
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
    ctx->recovery_latched        = false;
    ctx->compute_fault_seen_mask = 0u;
    ctx->pending_recovery        = false;
    ctx->input_fail_streak       = 0;
    ctx->input_fail_last_code    = FAULT_NONE;
    ctx->state = STATE_STARTUP;
    ctx->fault_entered_ms = now_ms;
}

/* ---------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */
void sm_init(sm_context_t *ctx, uint32_t now_ms)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->state = STATE_STARTUP;
    ctx->fault = FAULT_NONE;
    ctx->last_heartbeat_sent_ms = now_ms;
    ctx->last_valid_rx_ms = now_ms;

    /* Check BEFORE clearing: was this boot caused by the watchdog
     * timing out (previous cycle hung)? If so, remember it - we can't
     * call enter_fault() yet since that needs a real sensor_snapshot_t,
     * which doesn't exist until the first sm_run() call. Reported as
     * soon as one arrives, see STATE_STARTUP in sm_run(). */
    ctx->pending_watchdog_fault = __HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != 0;
    __HAL_RCC_CLEAR_RESET_FLAGS(); /* clear so next boot's check is accurate */

    hw_dac_init();
    hw_can_init();
    hw_uart_rx_arm();
    hw_set_mcu_gate(false); /* safe default before anything else runs */
}

bool sm_loop_ready(void)
{
    static uint32_t last_run_ms = 0;
    uint32_t now = HAL_GetTick();
    if ((now - last_run_ms) < SM_LOOP_PERIOD_MS) {
        return false;
    }
    last_run_ms = now;
    return true;
}

uint32_t sm_now_ms(void)
{
    return HAL_GetTick();
}

void sm_read_snapshot(sensor_snapshot_t *snap)
{
    uint32_t now = HAL_GetTick();
    snap->now_ms = now;

    uint16_t main_adc = 0, sub_adc = 0;
    hw_adc_read(ADC_CHANNEL_0, &main_adc); /* MAIN_ADC / PA0 */
    hw_adc_read(ADC_CHANNEL_1, &sub_adc);  /* SUB_ADC  / PA1 */
    snap->main_adc = main_adc;
    snap->sub_adc  = sub_adc;

    uint16_t steer_cmd = 0;
    bool steer_req = false;
    if (hw_can_try_receive(&steer_cmd, &steer_req, now)) {
        snap->can_value = steer_cmd;
        snap->can_boost_requested = steer_req;
    }
    /* can_value/can_boost_requested hold the last successfully decoded
     * values even on a cycle with no new frame - can_fresh below is
     * what actually reflects whether they're still timely. Per Ting:
     * if STEER_REQ==0, ignore can_value entirely and stay pass-through -
     * boost_condition_met() already checks can_boost_requested first,
     * before ever looking at the threshold, so this is handled correctly
     * downstream. */
    snap->can_fresh = (now - s_last_can_rx_ms) < CAN_MSG_TIMEOUT_MS;
}

void sm_run(sm_context_t *ctx, const sensor_snapshot_t *snap)
{
    hw_watchdog_refresh(); /* must happen every cycle - if this call stops
                             * happening (hung loop), the chip auto-resets */

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
        if (ctx->pending_watchdog_fault) {
            ctx->pending_watchdog_fault = false;
            enter_fault(ctx, FAULT_WATCHDOG_RESET, snap);
            break;
        }
        uart_send_status(ctx, snap, false);
        /* Raw (non-debounced) check on purpose - a bad sample here
         * should not be forgiven, self-test's whole point is
         * confirming the signal is trustworthy before entering service. */
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

        bool want_boost = boost_condition_met(snap, false);
        uart_send_status(ctx, snap, want_boost);

        /* This chip's OWN decision drives its OWN gate pin directly -
         * the AND-gate hardware, not UART trust, enforces that the
         * supervisor must independently agree before anything actually
         * reaches the EPS. */
        hw_set_mcu_gate(want_boost);
        ctx->mcu_gate_state = want_boost;

        if (want_boost) {
            ctx->state = STATE_BOOST_ACTIVE;
        }
        break;
    }

    case STATE_BOOST_ACTIVE: {
        if (!input_checks_ok_debounced(ctx, snap, &new_fault)) {
            enter_fault(ctx, new_fault, snap);
            break;
        }

        bool still_wants_boost = boost_condition_met(snap, true);
        uart_send_status(ctx, snap, still_wants_boost);

        hw_set_mcu_gate(still_wants_boost);
        ctx->mcu_gate_state = still_wants_boost;

        if (!still_wants_boost) {
            ctx->state = STATE_NORMAL_PASSTHROUGH;
            break;
        }

        /* Boost amount - PLACEHOLDER formula, approved as a starting
         * point: push MAIN up by a fixed offset, derive SUB to keep
         * MAIN+SUB at the expected sum (mirrors the correlation check
         * itself uses, and the "S2 mirrored out" idea from the
         * flowchart). Tune BOOST_OFFSET_COUNTS once real bench data
         * exists - replace this whole block once Ting confirms the
         * real formula. */
        {
            uint32_t dac_main = (uint32_t)snap->main_adc + BOOST_OFFSET_COUNTS;
            if (dac_main > 4095u) {
                dac_main = 4095u; /* clamp to 12-bit DAC max */
            }
            uint32_t dac_sub = (ADC_EXPECTED_SUM > dac_main)
                                ? (ADC_EXPECTED_SUM - dac_main)
                                : 0u;
            if (dac_sub > 4095u) {
                dac_sub = 4095u;
            }
            hw_dac_write((uint16_t)dac_main, (uint16_t)dac_sub);
        }
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
    case STATE_FAULT:              return "FAULT";
    default:                       return "UNKNOWN";
    }
}
