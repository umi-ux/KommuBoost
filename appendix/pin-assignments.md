# Pin Assignments & Constants

## Confirmed pin assignments — Main MCU (STM32G0B1CBT6)

| Pin | Net label | Purpose |
|---|---|---|
| PA0 | `MAIN_ADC` | Read conditioned torque MAIN signal |
| PA1 | `SUB_ADC` | Read conditioned torque SUB signal |
| PA4 | `DAC_MAIN` | DAC channel 1 — MAIN modified signal |
| PA5 | `DAC_SUB` | DAC channel 2 — SUB modified signal |
| PB8 | `CAN_RX` | FDCAN receive |
| PB9 | `CAN_TX` | FDCAN transmit |
| PC6 | `HEARTBEAT_OUT` | 50ms pulse to supervisor watchdog |
| PA9 | `UART_TX` | Diagnostic UART to supervisor |
| PA8 | `UART_RX` | Diagnostic UART from supervisor |
| PA13 | `MCU_SWDIO` | SWD data |
| PA14 | `MCU_SWDCLK` | SWD clock + BOOT0 |

## Confirmed pin assignments — Supervisor MCU (STM32G030F6P6TR)

| Pin | Net label | Purpose |
|---|---|---|
| PA0 | `MAIN_ADC` | Independent torque MAIN read |
| PA1 | `SUB_ADC` | Independent torque SUB read |
| PA2 | `HEARTBEAT_IN` | Main MCU watchdog heartbeat |
| PA3 | `UART_TX` | Diagnostic to main MCU |
| PA4 | `UART_RX` | Diagnostic from main MCU |
| PA5 | `GATE_ENABLE` | Arms output stage (to AND gate) |
| PA6 | `FORCE_PT` | Active safety signal (to AND gate) |
| PA7 | `5V_MON` | ECU 5V rail health monitor |
| PB0 | `FAULT_OUT` | Hardware output fault from LM393 |
| PA13 | `SWDIO` | SWD data |
| PA14 | `SWDCLK` | SWD clock + BOOT0 |

`FORCE_PT`/`GATE_ENABLE` ownership was previously disputed between the scenario flow document and the schematic — resolved: the **supervisor** owns both. See [Hardware Summary](../hardware/README.md#corrected-pin-ownership-of-force_pt--gate_enable).

## ADC constants

Derived from bench-measured torque sensor values and the confirmed 0.645 gain from the 10k/20k resistor divider AFE:

| Constant | Value |
|---|---|
| `ADC_MIN_VALID_COUNTS` | 950 |
| `ADC_MAX_VALID_COUNTS` | 3150 |
| `ADC_EXPECTED_SUM` | 4065 |
| `ADC_SUM_TOLERANCE` | 80 |

**Assumptions:** `VREF = 3.3V`, 12-bit ADC resolution. **Not yet confirmed** against actual ADC configuration — see [Open Items](../open-items/README.md#vref-confirmation-for-adc-constants).

## Boost thresholds (on CAN value field, not raw ADC)

| Transition | Threshold |
|---|---|
| Entry to `Boost_Active` | CAN value > 200 |
| Exit to `Normal_PassThrough` | CAN value < 185 |

## AFE gain

Confirmed gain: **0.645**, from a 10k/20k resistor divider. Independently verified via LTspice simulation.
