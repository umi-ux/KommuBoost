/* main_supervisor.h - SAFETY SUPERVISOR (STM32G030F6P6TR)
 *
 * Independent safety check. Computes its OWN approve/deny verdict from
 * ONLY its own readings (own ADC taps, own heartbeat pin read, own
 * FAULT_OUT read) and drives ONLY its own gate pin
 * (SUPERVISOR_GATE_ENABLE). Does NOT wait for or depend on what the
 * main MCU claims over UART - that link is diagnostic/secondary now,
 * matching the main MCU's equivalent design. The AND-gate hardware
 * downstream is what combines this chip's verdict with the main MCU's
 * independently-driven MCU_GATE_ENABLE pin - two separately-reasoned
 * decisions, not one decision mirrored onto two pins.
 *
 * *** OPEN GAP - no rail-voltage sense pin exists in the current
 * pinout. This was planned as a supervisor-only check but was never
 * actually wired into the schematic/CubeMX pin config. Dropped from
 * this file's checks entirely rather than faked - flag if
 * this is still wanted. ***
 */
#ifndef MAIN_SUPERVISOR_H
#define MAIN_SUPERVISOR_H

#include <stdint.h>
#include <stdbool.h>
#include "fault_codes.h"
#include "shared_protocol.h"

/* PLACEHOLDER tunables - independently chosen from the main MCU's,
 * confirm every one against hardware/Ting/bench data. */
#define SUP_ADC_MIN_VALID_COUNTS   620u
#define SUP_ADC_MAX_VALID_COUNTS   3410u
#define SUP_ADC_EXPECTED_SUM       4095u
#define SUP_ADC_SUM_TOLERANCE      100u

#define SUP_HEARTBEAT_TIMEOUT_MS   150u

#define SUP_FAULT_DEBOUNCE_SAMPLES 3u

/* Fixed period (ms) sup_run() should actually execute at - same
 * reasoning as the main MCU's SM_LOOP_PERIOD_MS. */
#define SUP_LOOP_PERIOD_MS         10u

typedef struct {
    uint16_t main_adc;
    uint16_t sub_adc;
    bool     heartbeat_pin_state;   /* raw read of HEARTBEAT input pin */
    bool     fault_out_asserted;    /* raw read of FAULT_OUT input pin -
                                      * hardware comparator's own verdict,
                                      * independent of any firmware check */
    uint32_t now_ms;
} sup_snapshot_t;

typedef struct {
    bool     last_heartbeat_pin_state;
    uint32_t last_heartbeat_change_ms;

    /* UART - diagnostic/secondary channel now, not part of the gating
     * decision. Kept for cross-visibility into what the main MCU is
     * seeing/deciding. */
    uint8_t  last_rx_seq;
    bool     have_valid_request;
    bool     req_want_boost;
    uint32_t last_valid_rx_ms;
    uint8_t  tx_seq;
    uint32_t last_uart_tx_ms;

    /* Debounce - streak tracks WHICH fault type is repeating. */
    uint8_t      input_fail_streak;
    fault_code_t input_fail_last_code;

    fault_code_t current_fault;
    bool         gate_state; /* this chip's OWN gate pin state */

    /* Set once at sup_init() if this chip just came back from an IWDG
     * timeout. Reported once via current_fault/UART on the first
     * sup_run() cycle, then cleared - see sup_run(). */
    bool         pending_watchdog_fault;
} sup_context_t;

void sup_init(sup_context_t *ctx, uint32_t now_ms);
void sup_run(sup_context_t *ctx, const sup_snapshot_t *snap);

/* Call every iteration of main()'s while(1) loop. Returns true exactly
 * when SUP_LOOP_PERIOD_MS has elapsed since the last true return. */
bool sup_loop_ready(void);

/* Current time in ms since boot (HAL_GetTick()). */
uint32_t sup_now_ms(void);

/* Fills a sup_snapshot_t with real ADC/GPIO readings for this cycle. */
void sup_read_snapshot(sup_snapshot_t *snap);

#endif /* MAIN_SUPERVISOR_H */
