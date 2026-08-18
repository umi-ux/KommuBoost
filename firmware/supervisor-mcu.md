# Supervisor MCU (`STM32G030F6P6TR`)

Files: `main_supervisor.h`, `main_supervisor.c`

## Role

The supervisor is the simpler, less busy chip: no CAN, no DAC, a narrower peripheral set than the main MCU. It computes its own approve/deny verdict from only its own evidence (own ADC taps, own heartbeat pin read, own `FAULT_OUT` read) and does not depend on what the main MCU reports over UART. This split, a simple chip holding real veto power independent of the busier chip, is a deliberate safety mechanism. See [Key Learnings](../key-learnings/README.md).

The supervisor has no fault-recovery state machine. It is continuously re-evaluated every cycle by design.

## Pin assignments

| Pin | Net label | Direction | Purpose |
|---|---|---|---|
| PA0 | `MAIN_ADC` | In | Independent torque MAIN read (own ADC tap) |
| PA1 | `SUB_ADC` | In | Independent torque SUB read |
| PA2 | `UART_TX` | Out | Diagnostic UART to main MCU |
| PA3 | `UART_RX` | In | Diagnostic UART from main MCU |
| PA4 | `HEARTBEAT` | In | Watches main MCU's PA12 pulse |
| PA6 | `SUPERVISOR_GATE_ENABLE` | Out | This chip's own gate signal, to the AND gate |
| PB0 | `FAULT_OUT` | In | Hardware fault signal from the LM393 comparator |
| PA13 | `SWDIO` | Debug | SWD programming data |
| PA14 | `SWDCLK` | Debug | SWD programming clock + BOOT0 |

No rail-voltage-sense pin exists in the current pinout. A supervisor-side power-rail self-check was planned but not wired into the schematic.

## Decision logic

```c
approve = own_ADC_checks_pass && heartbeat_ok && !FAULT_OUT_asserted
```

Drives `SUPERVISOR_GATE_ENABLE` to match `approve`, every cycle. UART is diagnostic-only in both directions and plays no part in this decision.

## Watchdog

IWDG active, same settings as the main MCU (prescaler 32, reload 500). A watchdog-caused reset is detected at boot and reported once via `current_fault` and the diagnostic UART frame. Because this chip has no fault-recovery state machine, the report does not force the gate low beyond whatever the live checks that same cycle already decide.

## Requirements before production use

- Resolve the missing rail-voltage-sense pin: either confirm the requirement is dropped, or add the schematic wiring needed to implement it
- Complete a bench-validation pass on `SUP_HEARTBEAT_TIMEOUT_MS`, `SUP_FAULT_DEBOUNCE_SAMPLES`, and the ADC range/correlation constants, independently from the main MCU's equivalents
- Decide, deliberately, whether this chip should carry any form of fault memory, or whether fully stateless operation is the correct final design for its role
