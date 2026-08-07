# Main MCU (`STM32G0B1CBT6`)

Files: `main_mcu.h`, `main_mcu.c`

## Role

- Owns the 5-state state machine (see [State Machine](state-machine.md))
- CAN-facing: receives boost commands from KommuAssist2
- Drives the DAC output during `Boost_Active`
- Runs always-on input validation (ADC range checks, stuck-value detection)
- Implements recovery-latch enforcement for the two-bucket fault policy (currently main-MCU-only — see [Fault Handling](fault-handling.md))

## Confirmed pin assignments

| Pin | Net label | Direction | Purpose |
|---|---|---|---|
| PA0 | `MAIN_ADC` | In | Read conditioned torque MAIN signal |
| PA1 | `SUB_ADC` | In | Read conditioned torque SUB signal |
| PA4 | `DAC_MAIN` | Out | DAC channel 1 — MAIN modified signal |
| PA5 | `DAC_SUB` | Out | DAC channel 2 — SUB modified signal |
| PB8 | `CAN_RX` | In | FDCAN receive |
| PB9 | `CAN_TX` | Out | FDCAN transmit |
| PC6 | `HEARTBEAT_OUT` | Out | 50ms pulse to supervisor watchdog |
| PA9 | `UART_TX` | Out | Diagnostic UART to supervisor |
| PA8 | `UART_RX` | In | Diagnostic UART from supervisor |
| PA13 | `MCU_SWDIO` | Debug | SWD programming data |
| PA14 | `MCU_SWDCLK` | Debug | SWD programming clock + BOOT0 |

> **Correction:** the main MCU does **not** own `FORCE_PT` or `GATE_ENABLE` — those are supervisor pins (see [Supervisor MCU](supervisor-mcu.md)). Earlier notes had `FORCE_PT=PA6`, `GATE_ENABLE=PA5`, and `HEARTBEAT_OUT=PA10` on the main MCU; this was wrong. It's been corrected against Kommu's hardware design study — `HEARTBEAT_OUT` is `PC6` on the main MCU, and `FORCE_PT`/`GATE_ENABLE` belong entirely to the supervisor. See [Hardware Summary](../hardware/README.md#corrected-pin-ownership-of-force_pt--gate_enable) for why this matters.

## ADC constants (AFE-derived)

Derived from bench-measured torque sensor values and the confirmed 0.645 gain from the 10k/20k resistor divider:

| Constant | Value |
|---|---|
| `ADC_MIN_VALID_COUNTS` | 950 |
| `ADC_MAX_VALID_COUNTS` | 3150 |
| `ADC_EXPECTED_SUM` | 4065 |
| `ADC_SUM_TOLERANCE` | 80 |

These assume `VREF = 3.3V` and 12-bit ADC resolution. **This still needs confirmation against the actual ADC configuration** — see [Open Items](../open-items/README.md#vref-confirmation-for-adc-constants).

## Register access

Uses the hand-written `reg_defs.h` (shared across both targets) covering only the GPIO and RCC peripherals actually needed. No HAL, no CubeMX, no CMSIS. See [Firmware Architecture](architecture.md#no-hal--register-level-approach) for why, and the `CRRCR` bug that was caught during development.

## DAC strategy

Per the design study, DAC output uses **Option B**:

```
DAC_MAIN = boost_value
DAC_SUB  = MAX_VALUE - boost_value   // guarantees MAIN + SUB = 5V always
```

This keeps the complementary relationship intact in firmware by construction, which is what prevents ECU fault C1513 (signal mismatch) from the firmware side — independent of the LM393 hardware check on the output side. Note this is the *target* strategy; it still needs to be implemented as part of the [boost amount formula](../open-items/README.md#boost-amount-formula) work.

## Known TODOs specific to the main MCU

- Boost amount formula (how `DAC_MAIN`/`DAC_SUB` are computed from `can_value`/`main_adc` while in `STATE_BOOST_ACTIVE`) is not yet implemented — this blocks real boost testing. See [Open Items](../open-items/README.md#boost-amount-formula).
- Ignition detection is not yet implemented.
- `SUB_ADC` stuck-check coverage is missing (only `MAIN_ADC` is currently watched).
