/* main_mcu.h - MAIN MCU (STM32G0B1CBT6)
 *
 * Runs the boost decision logic AND, under the updated split-pin gate
 * architecture, directly drives MCU_GATE_ENABLE from its own decision.
 * The supervisor independently drives SUPERVISOR_GATE_ENABLE from ITS
 * own decision. An AND gate downstream requires BOTH to agree before
 * the analog switch actually connects boosted output to the EPS - so
 * this chip does not need to wait for supervisor approval over UART
 * before acting; the hardware AND gate is what enforces the veto, not
 * software trust in a UART message. UART is now a secondary/diagnostic
 * channel, not the sole approval path (see shared_protocol.h).
 *
 * States match the approved Torque_Interceptor_State_Diagram_v2:
 *   Power_Off_NC_PassThrough -> hardware default, not represented here.
 *   Startup_SelfTest         -> STATE_STARTUP
 *   Normal_PassThrough       -> STATE_NORMAL_PASSTHROUGH
 *   Boost_Active             -> STATE_BOOST_ACTIVE
 *   Fault_Detected           -> STATE_FAULT
 *
 * *** TBC ***
 *   - CAN message: BO_ 464 STEERING_LKAS. STEER_REQ (1 bit) says
 *     whether to even look at STEER_CMD at all - if STEER_REQ==0,
 *     ignore STEER_CMD entirely and stay pass-through. STEER_CMD
 *     (11 bits) is the value compared against BOOST_THRESHOLD.
 *     See CAN_ID_STEERING_LKAS in this file for bit-position detail
 *     and an open question about signed vs unsigned interpretation
 *     still worth double-checking against a real captured frame.
 *
 * *** STILL UNCONFIRMED, FLAG ***
 *   - Boost amount formula (DAC_MAIN/DAC_SUB from can_value/main_adc)
 *     still undetermined. A separate flowchart suggested SUB is
 *     mirrored/derived from MAIN to preserve a target sum, rather than
 *     independently computed - worth confirming with Ting.
 *   - Rail-voltage sensing: no ADC channel exists for this on the main
 *     MCU per the generated CubeMX project - only the supervisor senses
 *     its own rail. Dropped from this file's checks accordingly.
 */
#ifndef MAIN_MCU_H
#define MAIN_MCU_H

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
#define ADC_STUCK_TIMEOUT_MS      500u

#define HEARTBEAT_PERIOD_MS       50u
#define CAN_MSG_TIMEOUT_MS        100u

#define BOOST_THRESHOLD             200u
#define BOOST_THRESHOLD_HYSTERESIS  15u

/* PLACEHOLDER - simple fixed offset approach, approved as a starting
 * point pending Ting's real formula. MAIN gets pushed up by this many
 * ADC counts; SUB is derived to preserve the MAIN+SUB=ADC_EXPECTED_SUM
 * invariant (matches the "S2 mirrored" idea from the flowchart) rather
 * than being independently computed. Tune this single number once
 * real bench data exists - no logic rewrite needed to change it. */
#define BOOST_OFFSET_COUNTS         30u

#define FAULT_DEBOUNCE_SAMPLES     3u
#define FAULT_RECOVERY_DEBOUNCE_MS 200u

/* Fixed period (ms) sm_run() should actually execute at - see
 * sm_loop_ready(). Keep comfortably faster than CAN_MSG_TIMEOUT_MS and
 * PROTO_LINK_TIMEOUT_MS. */
#define SM_LOOP_PERIOD_MS          10u

/* CAN message - CONFIRMED: BO_ 464 STEERING_LKAS
 *   SG_ STEER_REQ : 21|1@0+ (1,0) [0|1]     - 1 = boost request active, 0 = pass through, ignore STEER_CMD
 *   SG_ STEER_CMD : 7|11@0- (1,0) [0|255]   - the value to compare against BOOST_THRESHOLD
 * NOTE: DBC declares STEER_CMD as 11-bit SIGNED (@0-) but states range
 * [0|255] - these don't match (11-bit signed could be -1024..1023).
 * This file extracts the raw 11-bit value UNSIGNED (no sign extension)
 * on the assumption the real range is 0-255 as Ting stated and the
 * declared bit width just has headroom. *** VERIFY against a real
 * captured frame with a known STEER_CMD value before trusting this -
 * multi-byte bit extraction is an easy place to be subtly wrong. *** */
#define CAN_ID_STEERING_LKAS       464u

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

    uint8_t      input_fail_streak;
    fault_code_t input_fail_last_code;

    /* UART link - now a diagnostic/secondary channel, see shared_protocol.h */
    uint8_t      tx_seq;
    uint32_t     last_uart_tx_ms;
    uint32_t     last_valid_rx_ms;
    bool         supervisor_approved;   /* informational only now */
    fault_code_t supervisor_fault;

    /* Two-bucket recovery tracking - per-fault-type bitmask, drive-scoped */
    uint32_t     compute_fault_seen_mask;
    bool         recovery_latched;
    bool         pending_recovery;

    /* Own gate pin state - directly reflects this chip's own decision */
    bool         mcu_gate_state;

    /* Set once at sm_init() if the chip just came back from an IWDG
     * timeout (previous cycle hung and the watchdog force-reset us).
     * Reported as FAULT_WATCHDOG_RESET the first time sm_run() has a
     * real snapshot to pass to enter_fault() with, then cleared. */
    bool         pending_watchdog_fault;
} sm_context_t;

void sm_init(sm_context_t *ctx, uint32_t now_ms);
void sm_run(sm_context_t *ctx, const sensor_snapshot_t *snap);
void sm_notify_ignition_cycle(sm_context_t *ctx, uint32_t now_ms);
const char *sm_state_name(sm_state_t s);

/* Call every iteration of main()'s while(1) loop. Returns true exactly
 * when SM_LOOP_PERIOD_MS has elapsed since the last true return. */
bool sm_loop_ready(void);

/* Current time in ms since boot, from HAL's own SysTick-driven tick
 * (HAL_GetTick()) - no custom timebase needed now that HAL owns SysTick. */
uint32_t sm_now_ms(void);

/* Fills a sensor_snapshot_t with real ADC/CAN readings for this cycle.
 * Call this, then pass the result to sm_run(). */
void sm_read_snapshot(sensor_snapshot_t *snap);

#endif /* MAIN_MCU_H */
