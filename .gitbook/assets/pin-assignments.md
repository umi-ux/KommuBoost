# Pin Assignments & Constants

## Confirmed pin assignments, Main MCU (STM32G0B1CBT6)

| Pin | Net label | Purpose |
|---|---|---|
| PA0 | `MAIN_ADC` | Read conditioned torque MAIN signal |
| PA1 | `SUB_ADC` | Read conditioned torque SUB signal |
| PA4 | `DAC_MAIN` | DAC channel 1, boosted MAIN signal |
| PA5 | `DAC_SUB` | DAC channel 2, derived SUB signal |
| PA8 | `MCU_GATE_ENABLE` | This chip's own gate signal, to the AND gate |
| PA9 | `UART_TX` | Diagnostic UART to supervisor |
| PA10 | `UART_RX` | Diagnostic UART from supervisor |
| PA12 | `HEARTBEAT` | Toggled every 50ms, watched by supervisor |
| PB8 | `CAN_RX` | FDCAN receive |
| PB9 | `CAN_TX` | FDCAN transmit (unused) |
| PA13 | `SWDIO` | SWD data |
| PA14 | `SWDCLK` | SWD clock + BOOT0 |

## Confirmed pin assignments, Supervisor MCU (STM32G030F6P6TR)

| Pin | Net label | Purpose |
|---|---|---|
| PA0 | `MAIN_ADC` | Independent torque MAIN read (own ADC tap) |
| PA1 | `SUB_ADC` | Independent torque SUB read |
| PA2 | `UART_TX` | Diagnostic UART to main MCU |
| PA3 | `UART_RX` | Diagnostic UART from main MCU |
| PA4 | `HEARTBEAT` | Watches main MCU's PA12 pulse |
| PA6 | `SUPERVISOR_GATE_ENABLE` | This chip's own gate signal, to the AND gate |
| PB0 | `FAULT_OUT` | Hardware fault signal from LM393 comparator |
| PA13 | `SWDIO` | SWD data |
| PA14 | `SWDCLK` | SWD clock + BOOT0 |

> **Architecture note:** `MCU_GATE_ENABLE` and `SUPERVISOR_GATE_ENABLE` are two independently-driven signals feeding an AND gate, not one chip driving both. See [Firmware Architecture](../firmware/architecture.md#split-pin-gate-control). This replaces the earlier single-owner (`FORCE_PT`/`GATE_ENABLE`, both supervisor-driven) design entirely.

## ADC constants (main MCU)

| Constant | Current firmware value |
|---|---|
| `ADC_MIN_VALID_COUNTS` | 620 |
| `ADC_MAX_VALID_COUNTS` | 3410 |
| `ADC_EXPECTED_SUM` | 4095 |
| `ADC_SUM_TOLERANCE` | 100 |

**Assumptions:** `VREF+ = 3.3V` (confirmed against schematic), 12-bit ADC resolution.

> **Discrepancy flagged, not yet resolved:** an earlier doc draft listed a different set (`950 / 3150 / 4065 / 80`), described as "bench-measured." Not clear which is authoritative, see [Open Items](../open-items/README.md#adc-constant-discrepancy).

## Boost thresholds (on `STEER_CMD` field, confirmed CAN signal)

| Transition | Threshold |
|---|---|
| Entry to `Boost_Active` | `STEER_REQ`=1 AND `STEER_CMD` > 200 |
| Exit to `Normal_PassThrough` | `STEER_CMD` < 185 |

`STEER_REQ` = 0 always means pass-through, regardless of `STEER_CMD`'s value. See [Main MCU](../firmware/main-mcu.md#can-message--confirmed-with-ting).

## AFE gain

Confirmed gain: **0.645**, from a 10k/20k resistor divider. Independently verified via LTspice simulation.
