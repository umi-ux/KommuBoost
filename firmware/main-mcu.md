# Main MCU (`STM32G0B1CBT6`)

Files: `main_mcu.h`, `main_mcu.c`

## Role

- Owns the state machine (see [State Machine](state-machine.md))
- CAN-facing: receives the steering/boost message from KommuAssist2 over FDCAN
- Computes and drives the DAC boost output during `Boost_Active`
- Runs always-on input validation (ADC range, correlation, stuck-value checks on both MAIN and SUB)
- Drives its own gate pin (`MCU_GATE_ENABLE`) directly from its own decision, independent of the supervisor's UART traffic (see [Firmware Architecture](architecture.md))
- Refreshes the watchdog every cycle; detects and reports its own watchdog-caused resets

## Pin assignments

| Pin | Net label | Direction | Purpose |
|---|---|---|---|
| PA0 | `MAIN_ADC` | In | Read conditioned torque MAIN signal |
| PA1 | `SUB_ADC` | In | Read conditioned torque SUB signal |
| PA4 | `DAC_MAIN` | Out | DAC channel 1, boosted MAIN signal |
| PA5 | `DAC_SUB` | Out | DAC channel 2, derived SUB signal |
| PA8 | `MCU_GATE_ENABLE` | Out | This chip's own gate signal, to the AND gate |
| PA9 | `UART_TX` | Out | Diagnostic UART to supervisor |
| PA10 | `UART_RX` | In | Diagnostic UART from supervisor |
| PA12 | `HEARTBEAT` | Out | Toggled every 50ms; supervisor watches this to confirm the chip is alive |
| PB8 | `CAN_RX` | In | FDCAN receive |
| PB9 | `CAN_TX` | Out | FDCAN transmit (unused, this chip only listens) |
| PA13 | `SWDIO` | Debug | SWD programming data |
| PA14 | `SWDCLK` | Debug | SWD programming clock + BOOT0 |

## CAN message

`BO_ 464 STEERING_LKAS`, from `perodua_general_pt.dbc`:

| Signal | Bits | Meaning |
|---|---|---|
| `STEER_REQ` | 1 bit, byte 2 | 1 = boost requested, evaluate `STEER_CMD`. 0 = ignore `STEER_CMD`, stay pass-through. |
| `STEER_CMD` | 11 bits, spans byte 0-1 | The value compared against `BOOST_THRESHOLD` (200) once `STEER_REQ` is 1 |

The `STEER_CMD` bit-extraction is implemented per the DBC's motorola/big-endian convention. Multi-byte bit extraction requires validation against a live captured frame with a known value before it is treated as production-ready.

## ADC constants

| Constant | Value |
|---|---|
| `ADC_MIN_VALID_COUNTS` | 620 |
| `ADC_MAX_VALID_COUNTS` | 3410 |
| `ADC_EXPECTED_SUM` | 4095 |
| `ADC_SUM_TOLERANCE` | 100 |

Based on `VREF+ = 3.3V` (tied directly to the 3.3V rail on the schematic) and 12-bit ADC resolution.

## Boost output

```c
dac_main = main_adc + BOOST_OFFSET_COUNTS;   // currently 30
dac_sub  = ADC_EXPECTED_SUM - dac_main;      // derived, preserves the sum invariant
// both clamped to 0-4095 (12-bit DAC range)
```

`BOOST_OFFSET_COUNTS` is a single tunable constant. The value, and whether a flat offset is the right formula shape at all, requires bench validation against real hardware.

## Watchdog

IWDG active, refreshed every loop cycle (prescaler 32, reload 500, ~500ms timeout). A hang triggers an auto-reset; the next boot detects this via the reset-cause flag and reports `FAULT_WATCHDOG_RESET` once, through the normal two-bucket recovery policy.

## Requirements before production use

- Validate `STEER_CMD` bit-extraction against a live captured CAN frame with a known value
- Bench-tune `BOOST_OFFSET_COUNTS`, or replace the formula shape if a flat offset proves wrong
- Reconcile the ADC constants above against any independently bench-measured values
- Implement ignition-cycle detection, or formally confirm it is unnecessary given the board's power architecture
- Upgrade the UART checksum from a simple additive sum to a CRC before a production revision
- Complete a systematic bench-validation pass on all debounce/timeout/hysteresis constants
