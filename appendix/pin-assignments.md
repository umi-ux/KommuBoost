# Pin Assignments & Constants

> Pin tables below reflect `Schematic_Torque-Interceptor_2026-08-17.png`, the current authoritative source. See [Open Items](../open-items/README.md#pin-table-corrected-against-2026-08-17-schematic) for what changed from the previous revision of this book.

## Confirmed pin assignments — Main MCU (STM32G0B1CBT6)

| Pin | Net label | Purpose |
|---|---|---|
| PA0 (11) | `MAIN_ADC` | Read conditioned torque MAIN signal |
| PA1 (12) | `SUB_ADC` | Read conditioned torque SUB signal |
| PA4 (15) | `DAC_MAIN` | DAC channel 1 — MAIN modified signal |
| PA5 (16) | `DAC_SUB` | DAC channel 2 — SUB modified signal |
| PB8 (47) | `CAN_RX` | FDCAN receive |
| PB9 (48) | `CAN_TX` | FDCAN transmit |
| ~PB7 (46) ⚠️ | `MCU_GATE_ENABLE` | Main MCU's half of the two-key gate approval (new pin — see [Main MCU](../firmware/main-mcu.md)) |
| PA12 (33, remap PA10) | `HEARTBEAT` | 50ms pulse to supervisor watchdog |
| PA11 (32, remap PA9) | `UART_RX` | Diagnostic UART from supervisor |
| PA8 (28) | `UART_TX` | Diagnostic UART to supervisor |
| PA13 (35) | `MCU_SWDIO` | SWD data |
| PA14 (36) | `MCU_SWDCLK` | SWD clock + BOOT0 |

## Confirmed pin assignments — Supervisor MCU (STM32G030F8P6TR)

| Pin | Net label | Purpose |
|---|---|---|
| PA0 (7) | `MAIN_ADC` | Independent torque MAIN read |
| PA1 (8) | `SUB_ADC` | Independent torque SUB read |
| PA2 (9) | `UART_TX` | Diagnostic to main MCU |
| PA3 (10) | `UART_RX` | Diagnostic from main MCU |
| PA5 (12) | `HEARTBEAT` | Reads main MCU's watchdog heartbeat |
| PA6 (13) | `SUPERVISOR_GATE_ENABLE` | Supervisor's half of the two-key gate approval |
| PA12 (17, remap PA10) | `FAULT_OUT` | Hardware output fault from LM393 |
| PA13 (18) | `SWDIO` | SWD data |
| ~PA14 (19) | `SWDCLK` | SWD clock + BOOT0 |

`PA7` (previously documented as `5V_MON`) shows no net connection on the current schematic — flagged in [Open Items](../open-items/README.md#5v_mon-rail-monitoring-appears-missing).

`FORCE_PT`/`GATE_ENABLE` — the pair this book previously documented as both owned by the supervisor — no longer exist as separate nets on the current schematic. They've been replaced by two independent signals, one per chip (`MCU_GATE_ENABLE` from the main MCU, `SUPERVISOR_GATE_ENABLE` from the supervisor), both feeding the AND gate. See [Main MCU](../firmware/main-mcu.md) and [Supervisor MCU](../firmware/supervisor-mcu.md) for the corrected, per-chip pin tables, and [Complete Signal Flow](../firmware/complete-flow.md) for how the two signals combine.

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
