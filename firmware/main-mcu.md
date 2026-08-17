# Main MCU (`STM32G0B1CBT6`)

Files: `main_mcu.h`, `main_mcu.c`

## Role

- Owns the 5-state state machine (see [State Machine](state-machine.md))
- CAN-facing: receives boost commands from KommuAssist2
- Drives the DAC output during `Boost_Active`
- Runs always-on input validation (ADC range checks, stuck-value detection)
- Implements recovery-latch enforcement for the two-bucket fault policy (currently main-MCU-only — see [Fault Handling](fault-handling.md))

## Confirmed pin assignments

**Source: `Schematic_Torque-Interceptor_2026-08-17.png`, MCU sheet.** This supersedes earlier pin tables in this book — several pins changed since the last revision (noted below).

| Pin | Net label | Direction | Purpose |
|---|---|---|---|
| PA0 (11) | `MAIN_ADC` | In | Read conditioned torque MAIN signal |
| PA1 (12) | `SUB_ADC` | In | Read conditioned torque SUB signal |
| PA4 (15) | `DAC_MAIN` | Out | DAC channel 1 — MAIN modified signal |
| PA5 (16) | `DAC_SUB` | Out | DAC channel 2 — SUB modified signal |
| PB8 (47) | `CAN_RX` | In | FDCAN receive |
| PB9 (48) | `CAN_TX` | Out | FDCAN transmit |
| ~PB7 (46) ⚠️ | `MCU_GATE_ENABLE` | Out | **New pin, not previously documented.** Main MCU's own half of the two-key AND-gate approval — see note below. Top-side pin, exact number not fully legible on the schematic image; verify in EasyEDA before flashing. |
| PA12 (33, remapped as PA10) | `HEARTBEAT` | Out | 50ms pulse to supervisor watchdog. **Changed from `PC6`** in earlier notes. |
| PA11 (32, remapped as PA9) | `UART_RX` | In | Diagnostic UART from supervisor. **Direction/pin changed** — earlier notes had `UART_RX` on `PA8`. |
| PA8 (28) | `UART_TX` | Out | Diagnostic UART to supervisor. **Direction/pin changed** — earlier notes had `UART_TX` on `PA9`. |
| PA13 (35) | `MCU_SWDIO` | Debug | SWD programming data |
| PA14 (36) | `MCU_SWDCLK` | Debug | SWD programming clock + BOOT0 |

> **What changed from the previous pin table, and why it matters:** `HEARTBEAT` moved off `PC6`, and `UART_TX`/`UART_RX` swapped pins *and* effectively swapped direction relative to what was documented before. If any firmware or test harness code was written against the old table, it needs to be re-checked against this one before flashing. See [Open Items](../open-items/README.md#pin-table-corrected-against-2026-08-17-schematic).
>
> **`MCU_GATE_ENABLE` is new to this book.** Earlier docs only described the supervisor driving both AND-gate inputs (`FORCE_PT`/`GATE_ENABLE`). The current schematic shows the AND gate driven by one pin from **each** chip — `MCU_GATE_ENABLE` from the main MCU and `SUPERVISOR_GATE_ENABLE` from the supervisor (see [Supervisor MCU](supervisor-mcu.md)) — which is a stronger two-key arrangement than previously documented: neither chip alone can assert both AND-gate inputs. This matches the behavior described in [Complete Signal Flow](complete-flow.md).

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
