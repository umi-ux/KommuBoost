/* fault_codes.c - shared between main MCU and supervisor builds */
#include <fault_codes.h>

const char *fault_name(fault_code_t f)
{
    switch (f) {
    case FAULT_NONE:             return "NONE";
    case FAULT_SELFTEST_FAILED:  return "SELFTEST_FAILED";
    case FAULT_SIGNAL_MISMATCH:  return "SIGNAL_MISMATCH";
    case FAULT_OUT_OF_RANGE:     return "OUT_OF_RANGE";
    case FAULT_SIGNAL_STUCK:     return "SIGNAL_STUCK";
    case FAULT_HEARTBEAT_LOST:   return "HEARTBEAT_LOST";
    case FAULT_CAN_TIMEOUT:      return "CAN_TIMEOUT";
    case FAULT_POWER_RAIL:       return "POWER_RAIL";
    case FAULT_OUTPUT_MISMATCH:  return "OUTPUT_MISMATCH";
    case FAULT_DRIVER_OVERRIDE:  return "DRIVER_OVERRIDE";
    case FAULT_WATCHDOG_RESET:   return "WATCHDOG_RESET";
    case FAULT_LINK_TIMEOUT:     return "LINK_TIMEOUT";
    case FAULT_SUPERVISOR_DENIED:return "SUPERVISOR_DENIED";
    default:                     return "UNKNOWN";
    }
}
