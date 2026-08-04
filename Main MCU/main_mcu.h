/* torque_interceptor_sm.h - MAIN MCU (STM32G0B1CBT6)
 *
 * Runs the boost decision logic. Does NOT drive FORCE_PT/GATE_ENABLE -
 * those pins physically belong to the safety supervisor (STM32G030),
 * per the schematic. This MCU can only REQUEST boost over UART; the
 * supervisor independently re-checks and has final veto.
 *
 * NOTE: signal_flow_all_scenarios.html (your scenario walkthrough) says
 * in one place that the main MCU "raises FORCE_PT" directly. That
 * contradicts both the schematic (FORCE_PT lives on the Supervisor
 * sheet) and the same document's own fault scenario (which has the
 * SUPERVISOR pulling FORCE_PT low). Firmware follows the schematic -
 * confirm with Ting which is actually true in hardware.
 *
 * States match the approved Torque_Interceptor_State_Diagram_v2:
 *   Power_Off_NC_PassThrough -> hardware default, no MCU running, not
 *                               represented here on purpose.
 *   Startup_SelfTest         -> STATE_STARTUP
 *   Normal_PassThrough       -> STATE_NORMAL_PASSTHROUGH
 *   Boost_Active             -> STATE_BOOST_ACTIVE
 *   Fault_Detected           -> STATE_FAULT
 *
 * Hardware assumptions (confirm against schematic/bench before trusting):
 *   - MAIN/SUB torque signal: ~2.5V neutral, valid range ~1.0-4.0V.
 *   - MAIN_ADC + SUB_ADC should sum to a constant - characterize this
 *     against real bench data, ADC_EXPECTED_SUM below is a placeholder.
 *   - Boost gate: CAN "boost requested" flag + can_value field, checked
 *     against BOOST_THRESHOLD. CONFIRMED with Ting: threshold applies
 *     to the CAN value field, not the local MAIN_ADC reading.
 *   - The actual boost AMOUNT (how DAC_MAIN/DAC_SUB are computed from
 *     can_value / main_adc) is NOT yet determined - left as a TODO in
 *     the .c file on purpose. Do not assume the 1.5x gain mentioned in
 *     your scenario doc applies here - that's the fixed op-amp scaling
 *     stage (3.3V DAC -> 5V ECU), a different thing from the boost
 *     torque calculation itself.
 */
#ifndef TORQUE_INTERCEPTOR_SM_H
#define TORQUE_INTERCEPTOR_SM_H

#include <stdint.h>
#include <stdbool.h>
#include "fault_codes.h"
#include "shared_protocol.h"

/* ---------------------------------------------------------------------
 * Tunables - PLACEHOLDER values, confirm every one against hardware/Ting
 * --------------------------------------------------------------------- */
#define ADC_MIN_VALID_COUNTS      620u    /* ~1.0V on 12-bit/3.3V ADC after divider */
#define ADC_MAX_VALID_COUNTS      3410u   /* ~4.0V equivalent */
#define ADC_EXPECTED_SUM          4095u   /* MAIN_ADC + SUB_ADC target - characterize */
#define ADC_SUM_TOLERANCE         100u
#define ADC_STUCK_TIMEOUT_MS      500u    /* re-check against real driving data */

#define HEARTBEAT_PERIOD_MS       50u
#define CAN_MSG_TIMEOUT_MS        100u

/* Boost threshold with hysteresis so a value hovering near the edge
 * doesn't flicker in and out of boost every cycle. Enter boost above
 * BOOST_THRESHOLD; once active, only DROP OUT below
 * (BOOST_THRESHOLD - BOOST_THRESHOLD_HYSTERESIS). Both PLACEHOLDER -
 * the gap size should be picked from real CAN value noise/jitter on
 * the bench, not guessed. */
#define BOOST_THRESHOLD             200u   /* out of 255 CAN scanner units - CONFIRMED vs CAN field */
#define BOOST_THRESHOLD_HYSTERESIS  15u    /* PLACEHOLDER */

#define RAIL_5V_MIN_MV            4750u
#define RAIL_5V_MAX_MV            5250u

/* How many CONSECUTIVE bad samples an input check must see before it's
 * treated as a real fault rather than noise. This is what actually
 * implements "single ADC noise is transient" from the confirmed policy -
 * a lone bad sample gets absorbed here and never even becomes a logged
 * fault event. PLACEHOLDER - pick from real bench noise characterization. */
#define FAULT_DEBOUNCE_SAMPLES     3u

/* How long to sit in FAULT before attempting an auto-recovery
 * transition back to STARTUP, for classes that allow it. PLACEHOLDER. */
#define FAULT_RECOVERY_DEBOUNCE_MS 200u

/* ---------------------------------------------------------------------
 * States
 * --------------------------------------------------------------------- */
typedef enum {
    STATE_STARTUP = 0,
    STATE_NORMAL_PASSTHROUGH,
    STATE_BOOST_ACTIVE,
    STATE_FAULT
} sm_state_t;

typedef struct {
    uint16_t main_adc;
    uint16_t sub_adc;
    uint16_t rail_5v_mv;
    bool     can_boost_requested;
    bool     can_fresh;
    uint16_t can_value;
    uint32_t now_ms;
} sensor_snapshot_t;

typedef struct {
    sm_state_t   state;
    fault_code_t fault;

    uint16_t     last_main_adc;
    uint32_t     last_main_adc_change_ms;
    uint16_t     last_sub_adc;
    uint32_t     last_sub_adc_change_ms;
    uint32_t     last_heartbeat_sent_ms;
    uint32_t     fault_entered_ms;

    /* Debounce - consecutive-failure streak before a raw check failure
     * is allowed to become a real fault. Reset on pass, on STARTUP
     * entry, and on entering FAULT. Tracks WHICH fault type is
     * streaking (input_fail_last_code) so that e.g. one rail glitch +
     * one range glitch + one correlation glitch in a row do NOT get
     * mistaken for the same problem persisting 3 cycles - the streak
     * only counts consecutive occurrences of the SAME fault code. */
    uint8_t      input_fail_streak;
    fault_code_t input_fail_last_code;

    /* UART link to supervisor - this MCU can only REQUEST boost */
    uint8_t      tx_seq;
    uint32_t     last_uart_tx_ms;
    uint32_t     last_valid_rx_ms;
    bool         supervisor_approved;
    fault_code_t supervisor_fault;

    /* Two-bucket recovery tracking (confirmed policy, see fault_codes.h).
     * Drive-scoped: cleared only on ignition cycle.
     *
     * compute_fault_seen_mask: one bit per fault_code_t value (FAULT_COUNT
     * is well under 32, so a uint32_t bitmask is enough). A bit is set
     * the FIRST time that specific compute-integrity fault occurs this
     * drive. If that SAME fault type occurs again before the next
     * ignition cycle - regardless of what other faults happened in
     * between - it escalates. This replaces remembering only the single
     * most recent compute fault, which could never notice a fault type
     * recurring if a different fault type happened in between. */
    uint32_t     compute_fault_seen_mask;
    bool         recovery_latched;
    bool         pending_recovery;
} sm_context_t;

void sm_init(sm_context_t *ctx, uint32_t now_ms);
void sm_run(sm_context_t *ctx, const sensor_snapshot_t *snap);
void sm_notify_ignition_cycle(sm_context_t *ctx, uint32_t now_ms);
const char *sm_state_name(sm_state_t s);

#endif /* TORQUE_INTERCEPTOR_SM_H */
