# State Machine

The main MCU owns a 4-state machine. `Power_Off_NC_PassThrough` is a hardware-only condition, not a firmware state.

## State diagram

The main MCU's full state machine, Power Off through Startup, Normal Pass-through, Boost Active, and Fault Detected. The nested arcs at the top all represent the same underlying trigger, total loss of the board's 5V supply, shown from each active state since it can happen at any point. Note the Power Off box's description: it does not claim the board fails safely here, it states plainly that the switch goes open and no signal reaches the ECU, this is the same unresolved power gap noted throughout the docs, shown here at the state-machine level. The red arrow along the bottom is the soft-retry path (Fault Detected back to Startup) for transient or first-time compute-integrity faults, recovering without needing a full power cycle, distinct from the top arcs which require an actual power cycle to clear.

## State descriptions

| State                      | Description                                                                                                                                                                                                                                                                                                                                            |
| -------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `Power_Off_NC_PassThrough` | Hardware default. The NC analog switch provides pass-through purely from being unpowered. Covers the switch losing its control signal, not the board losing physical continuity with the signal path entirely, see [Firmware Architecture](architecture.md#power-caveat-fail-safe-with-zero-power-covers-logicmcu-failure-not-physical-disconnection). |
| `Startup_SelfTest`         | Entered on boot. Runs one raw, non-debounced input check. A bad sample here is not forgiven, since the point is confirming the signal is trustworthy before entering service.                                                                                                                                                                          |
| `Normal_PassThrough`       | Continuous debounced validation running every cycle. Torque signal passes to the ECU unmodified. `MCU_GATE_ENABLE` driven LOW every cycle.                                                                                                                                                                                                             |
| `Boost_Active`             | Boost engaged. `MCU_GATE_ENABLE` driven HIGH every cycle. DAC output active, see [Main MCU](main-mcu.md#boost-output).                                                                                                                                                                                                                                 |
| `Fault_Detected`           | Entered from any state on a validation failure. Recovery per the two-bucket policy, see [Fault Handling](fault-handling.md).                                                                                                                                                                                                                           |

## Boost entry and exit

Gated on two signals from `BO_ 464 STEERING_LKAS` (see [Main MCU](main-mcu.md#can-message)):

1. `STEER_REQ`: if 0, `STEER_CMD` is ignored entirely and the state stays `Normal_PassThrough`.
2. `STEER_CMD`, evaluated only once `STEER_REQ` = 1:
   * Entry: `STEER_CMD` > 200 transitions to `Boost_Active`
   * Exit: `STEER_CMD` < 185 (200 minus a 15-count hysteresis band) transitions back to `Normal_PassThrough`

The 185-200 gap prevents state chatter when the value sits near the threshold.

## Gate output

The state machine drives `MCU_GATE_ENABLE` directly from its own logic, independent of the supervisor. Whether boost actually reaches the EPS is decided downstream, in the AND-gate hardware, by combining this chip's pin with the supervisor's independently-computed `SUPERVISOR_GATE_ENABLE`. See [Firmware Architecture](architecture.md).

## Why input validation is always-on, but boost computation is gated

Input validation runs every cycle regardless of state, so a sensor fault is never missed just because no boost was requested. Boost computation only engages once `STEER_REQ`/`STEER_CMD` conditions are met. This is the deliberate split between the always-on safety monitoring function and the gated ADAS function.
