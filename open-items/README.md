# Open Items & TODOs

This is the punch list for anyone picking up the project. Items are roughly ordered by how much they block other work.

## UART protocol between MCUs

**Status:** Frame format drafted, **unconfirmed with Ting**. This is the largest remaining chunk of firmware work.

Blocks: supervisor-side recovery-latch enforcement, and any feature that needs the two MCUs to agree on fault/recovery state.

**Action needed:** Review draft frame format with Ting and get sign-off before implementing further.

## Boost amount formula

**Status:** Not implemented. Critical TODO blocking real boost testing.

This is the formula for computing `DAC_MAIN`/`DAC_SUB` from `can_value`/`main_adc` while in `STATE_BOOST_ACTIVE`. Without this, `Boost_Active` state can be entered but produces no meaningful output.

**Action needed:** Define the formula (likely needs Ting's input on the desired boost curve/gain) and implement.

## `fault_classify()` table — 7 of 10 entries pending

**Status:** 3 confirmed, 7 pending Ting's FMEDA review, currently placed conservatively as placeholders.

Pending entries include `SELFTEST_FAILED`, `SIGNAL_MISMATCH`, `POWER_RAIL`, `DRIVER_OVERRIDE`, and others.

**Action needed:** FMEDA review with Ting to confirm bucket assignment (transient/environmental vs. compute-integrity) for each.

## Supervisor-side recovery-latch enforcement

**Status:** Currently only implemented on the main MCU.

For the supervisor's independence to be a meaningful safety mechanism (rather than just trusting the main MCU's bookkeeping), it likely needs to independently enforce the same two-bucket retry/escalation policy.

**Action needed:** Design and implement on the supervisor once the UART protocol is confirmed (the supervisor needs a way to know fault history to enforce this).

## ~~`FORCE_PT`/`GATE_ENABLE` pin ownership contradiction~~ — RESOLVED

**Status:** Resolved by Kommu's formal hardware design study (REV 1.0, July 2026).

The **supervisor** owns both `FORCE_PT` (PA6) and `GATE_ENABLE` (PA5), feeding a hardware AND gate (SN74LVC1G08) whose output drives the analog switch. The main MCU does not own these pins. Earlier main-MCU pin assignments (`FORCE_PT=PA6`, `GATE_ENABLE=PA5`, `HEARTBEAT_OUT=PA10`) were incorrect and have been corrected throughout this book — see [Firmware: Main MCU](../firmware/main-mcu.md) and [Firmware: Supervisor MCU](../firmware/supervisor-mcu.md).

No further action needed on this item.

## Ignition detection

**Status:** Not implemented.

Needed for drive-cycle tracking, which the two-bucket fault recovery policy depends on (retries are scoped "per drive cycle").

**Action needed:** Define and implement ignition detection method (likely a GPIO input or CAN signal — needs confirmation).

## `PLACEHOLDER` tunable values

**Status:** Multiple values throughout the firmware are marked `PLACEHOLDER`, awaiting bench validation.

**Action needed:** Systematic bench validation pass once hardware is available for testing, replacing each `PLACEHOLDER` with a validated value.

## `SUB_ADC` stuck-check coverage

**Status:** Missing. Only `MAIN_ADC` is currently watched for stuck values.

**Action needed:** Extend stuck-value detection logic to cover `SUB_ADC` as well.

## Supervisor power rail self-check

**Status:** Architectural gap identified, not yet designed or implemented.

**Action needed:** Design a self-check mechanism for the supervisor's own power rail.

## VREF confirmation for ADC constants

**Status:** ADC constants (`ADC_MIN_VALID_COUNTS`, `ADC_MAX_VALID_COUNTS`, `ADC_EXPECTED_SUM`, `ADC_SUM_TOLERANCE`) assume `VREF = 3.3V` and 12-bit resolution. Not yet confirmed against actual ADC configuration.

**Action needed:** Confirm actual ADC config (VREF source, resolution setting) matches the assumption these constants were derived under.

## Hardware open questions (from design study)

These four questions are explicitly logged in the design study as awaiting Ting's answer, needed before PCB layout proceeds.

### Is boost threshold "200" in scanner units or raw ADC value?

**Why it matters:** determines the firmware threshold logic.
**Impact if not answered:** boost could activate at the wrong actual torque level.
**Note:** this is the same ambiguity flagged elsewhere in this book re: whether the boost threshold applies to the CAN value field or raw ADC counts — this is the authoritative place the question originates from.

### What exactly is the TS pin on the torque sensor connector?

**Why it matters:** may need an extra connector pin and schematic changes.
**Impact if not answered:** a signal could be missed entirely — unknown behavior.

### How much current can safely be drawn from the ECU's 5V VDD?

**Why it matters:** verifies the power budget is safe for the PCB to tap.
**Impact if not answered:** could disturb the sensor supply or trigger an ECU fault.

### Should the signal correlation check also exist in a hardware comparator, or is software sufficient?

**Why it matters:** affects the Output Submodule sheet design.
**Impact if not answered:** potential gap in output-path fault coverage — though note the LM393 resistor-averaging circuit already provides an independent hardware check on MAIN_OUT/SUB_OUT, so this question may be about whether *additional* hardware correlation checking is needed elsewhere (e.g. at the ADC input stage), not about the output stage, which is already covered.

## Part number correction

**Status:** Resolved. The analog switch is **TS5A23157**, confirmed against the BOM in the hardware design study. Earlier notes had **TS5A23159** — this was incorrect and has been corrected throughout this book.

## Priority summary

| Item | Blocks | Needs |
|---|---|---|
| UART protocol | Supervisor recovery-latch enforcement | Ting sign-off |
| Boost amount formula | Real boost testing | Ting input on boost curve |
| `FORCE_PT`/`GATE_ENABLE` ownership | Finalizing either MCU's veto-pin firmware | Hardware verification |
| `fault_classify()` table | Confident fault handling | Ting's FMEDA review |
| Ignition detection | Drive-cycle-scoped retry logic | Design decision |
