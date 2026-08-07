# Safety Architecture

Source: Section 3 of the hardware design study. This is the safety-goal-by-safety-goal and principle-by-principle mapping from Kommu's requirements document to what's actually implemented in hardware.

## Safety goals addressed

| Safety goal | How it's addressed |
|---|---|
| Prevent unintended steering assist | Dual gate logic — AND gate requires both `FORCE_PT` and `GATE_ENABLE` HIGH |
| Prevent loss of driver steering authority | NC switch default — OEM signal always available via a hardware path |
| Prevent sustained wrong-direction assist | LM393 output monitor detects a MAIN+SUB mismatch → immediate pass-through |
| Prevent modification when vehicle state unknown | CAN timeout detection in supervisor firmware |
| Prevent modification during fault | Supervisor forces `FORCE_PT` = LOW on any fault condition |
| Immediate reversion on fault | Hardware AND gate — pulling either input LOW forces instant pass-through |
| No unsafe behavior during startup/reset | `BOOT0` pull-down ensures a clean boot; NC switch is active throughout startup |
| No single fault causing sustained wrong output | Dual gate + independent supervisor + LM393 hardware monitor — three independent layers |
| Diagnostic fault logging | UART between the two MCUs; CAN reporting |

## Non-negotiable design principles compliance

| Principle | Implementation |
|---|---|
| OEM pass-through is the default state | TS5A23157 NC switch — physically connected by default |
| No software-only safety claim | Hardware AND gate + LM393 comparator — both independent of all firmware |
| No CAN-command-only torque modification | CAN request → main MCU calculates → supervisor independently validates → AND gate |
| No active function without driver-context override | Boost only active when KommuAssist2 explicitly requests it via CAN |
| No single fault causing sustained wrong output | Multiple independent layers: AND gate, supervisor, LM393 |

## Why three independent layers, not one

The design deliberately doesn't rely on any single mechanism:

1. **Supervisor firmware** — independently reads `MAIN_ADC`/`SUB_ADC`, can drop `FORCE_PT` on any fault it detects, monitors a heartbeat from the main MCU
2. **Hardware AND gate** — even if supervisor firmware has a bug that leaves one approval signal stuck HIGH, the *other* signal still has to independently go HIGH too. Even if all firmware on both chips crashed simultaneously, the gate defaults low.
3. **LM393 hardware comparator** — watches the actual output pins (`MAIN_OUT`, `SUB_OUT`) after the switch, completely independent of both MCUs. If the complementary relationship breaks, `FAULT_OUT` goes low regardless of what either MCU's firmware believes is happening.

This layering is what allows the "no software-only safety claim" principle to actually be true rather than aspirational — see [Key Learnings](../key-learnings/README.md#supervisor-holds-veto-is-correct-for-asil-b) for the reasoning that generalizes from this.

## Strong alignment points (from gap analysis)

| Requirement | Implementation |
|---|---|
| Concept C — Safety-Partitioned Interposer | Separate control MCU + safety supervisor MCU + hardware AND gate |
| Independent safety supervisor | STM32G030 as a separate physical MCU with its own ADC, firmware, and GPIO |
| Hardware pass-through guarantee | TS5A23157 NC switch — physically connected with zero power, zero firmware |
| No software-only safety claim | Hardware AND gate + LM393 comparator |
| Dual-channel correlation | MAIN+SUB sum check in both MCU firmware *and* LM393 hardware |
| Output stage fault detection | LM393 resistor-averaging circuit monitors the MAIN_OUT + SUB_OUT relationship |
| CAN safety gating | CAN request → main MCU → supervisor validates independently → AND gate |
| Heartbeat watchdog | Main MCU sends a 50ms pulse; supervisor forces pass-through if the pulse stops |
| Power rail monitoring | Supervisor reads `5V_MON` via a voltage divider |
| OEM pass-through is default | Hardwired via TS5A23157 NC default state |
