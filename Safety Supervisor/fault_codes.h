/* fault_codes.h
 *
 * Shared fault vocabulary + the two-bucket recovery policy classification.
 * Used by BOTH the main MCU (STM32G0B1CBT6) and the safety supervisor
 * (STM32G030F6P6TR) so a fault raised by either side speaks the same
 * language in logs.
 *
 * CONFIRMED POLICY (you + Ting):
 *   - Transient / environmental (CAN timeout, single ADC noise) ->
 *     auto-recover, no restriction.
 *   - Compute-integrity (MCU heartbeat loss, watchdog reset, output
 *     stuck) -> ONE auto-recovery attempt. If the SAME fault type
 *     recurs within the same drive (before the next ignition cycle),
 *     escalate to ignition-cycle-required.
 *   - Recovery always routes back through self-test (STARTUP state).
 *     Reaching a safe state on fault detection satisfies FTTI - how
 *     fast you recover afterward is an availability question, not a
 *     safety-timing one. That's an FMEDA documentation item, not a
 *     firmware behavior item.
 *
 * ASSUMPTION FLAG: the classify() mapping below is a starting proposal.
 * Anything marked "fails toward stricter bucket" is a placeholder until
 * the FMEDA says otherwise - confirm each one with Ting before treating
 * this as locked.
 */
#ifndef FAULT_CODES_H
#define FAULT_CODES_H

typedef enum {
    FAULT_NONE = 0,
    FAULT_SELFTEST_FAILED,
    FAULT_SIGNAL_MISMATCH,     /* MAIN_ADC + SUB_ADC out of tolerance       */
    FAULT_OUT_OF_RANGE,        /* MAIN or SUB outside valid voltage window  */
    FAULT_SIGNAL_STUCK,        /* frozen ADC reading                        */
    FAULT_HEARTBEAT_LOST,      /* supervisor stopped seeing main MCU pulse  */
    FAULT_CAN_TIMEOUT,         /* no fresh CAN message                      */
    FAULT_POWER_RAIL,          /* 5V_MON out of range                       */
    FAULT_OUTPUT_MISMATCH,     /* output monitor (LM393) disagrees w/ DAC   */
    FAULT_DRIVER_OVERRIDE,     /* driver torque overpowered the assist      */
    FAULT_WATCHDOG_RESET,      /* MCU came back up via IWDG/WWDG reset      */
    FAULT_LINK_TIMEOUT,        /* main<->supervisor UART link went stale    */
    FAULT_SUPERVISOR_DENIED,   /* supervisor's own checks disagreed         */
    FAULT_COUNT
} fault_code_t;

typedef enum {
    FAULT_CLASS_NONE = 0,
    FAULT_CLASS_TRANSIENT,
    FAULT_CLASS_COMPUTE_INTEGRITY
} fault_class_t;

/* CONFIRM WITH TING - this table IS the FMEDA-relevant decision. Every
 * fault lands in exactly one bucket; nothing should fall through
 * silently, which is why the default case below deliberately picks the
 * STRICTER bucket rather than the lenient one. */
static inline fault_class_t fault_classify(fault_code_t f)
{
    switch (f) {
    /* --- transient / environmental: auto-recover, no restriction --- */
    case FAULT_CAN_TIMEOUT:
    case FAULT_LINK_TIMEOUT:
        return FAULT_CLASS_TRANSIENT;

    /* --- compute-integrity: one retry, then escalate on repeat --- */
    case FAULT_HEARTBEAT_LOST:
    case FAULT_WATCHDOG_RESET:
    case FAULT_OUTPUT_MISMATCH:
        return FAULT_CLASS_COMPUTE_INTEGRITY;

    /* --- everything below is NOT yet a confirmed classification.
     * Defaulting to compute-integrity (stricter) on purpose - flag
     * these explicitly with Ting rather than assume they're transient: */
    case FAULT_SELFTEST_FAILED:
    case FAULT_SIGNAL_MISMATCH:
    case FAULT_OUT_OF_RANGE:
    case FAULT_SIGNAL_STUCK:
    case FAULT_POWER_RAIL:
    case FAULT_DRIVER_OVERRIDE:
    case FAULT_SUPERVISOR_DENIED:
    default:
        return FAULT_CLASS_COMPUTE_INTEGRITY;
    }
}

const char *fault_name(fault_code_t f);

#endif /* FAULT_CODES_H */
