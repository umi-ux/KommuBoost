# Hardware Summary

The hardware phase of this project is complete. This document is the authoritative source for hardware decisions; this page and its sub-pages summarize it for firmware engineering and handover.

## Signal path

The board sits inline between the Perodua EPS torque sensor and the EPS ECU:

```
EPS ECU  <──[Interceptor PCB]──>  Torque Sensor
```

Default state (no power, or the analog switch not actuated): **OEM pass-through**, hardware-guaranteed, no firmware required. Any fault, uncertainty, timeout, power issue, software error, signal mismatch, or diagnostic failure returns the ECU to the original torque sensor signal without delay.

> **Prototype/study phase only.** Public-road deployment requires functional safety assessment, legal review, vehicle-level validation, and regulatory approval, not yet performed.

## Confirmed signal characteristics

| Parameter | Value |
|---|---|
| Sensor type | Analog, 0–5V |
| Signal channels | MAIN + SUB (dual, complementary) |
| MAIN neutral voltage | ~2.5V at zero torque |
| MAIN signal range | 1.0V – 4.0V |
| MAIN + SUB relationship | Always sums to ~5V |
| ECU supply (VDD) | 5V, originates from ECU |
| Scanner value range | 0–255 (8-bit ADC inside ECU) |
| Boost threshold | `STEER_CMD` (CAN field) > 200, see [Main MCU](../firmware/main-mcu.md#can-message) |
| Fault code C1513 | Signal mismatch (MAIN + SUB ≠ 5V) |
| Fault code C1515 | Signal stuck at 0 or 255 |

## Power architecture

The PCB is powered from the ECU's 5V VDD pin, not the 12V battery:

- No 12V step-down converter needed, simpler design
- Cleaner power, already regulated by the ECU
- PCB taps a small amount of ECU 5V while passing the full 5V through to the sensor unchanged
- 5V regulated to 3.3V on-board via **AMS1117-3.3** LDO for all digital chips

## Key components

| Component | Part | Purpose |
|---|---|---|
| Main MCU | STM32G0B1CBT6 (LQFP-48) | Control path, calculates boost values, drives DAC outputs |
| Supervisor MCU | STM32G030F6P6TR (TSSOP-20) | Safety path, independently monitors signals, can force pass-through without relying on main MCU firmware |
| Op-amp (AFE + DAC scaling) | MCP6002DRG (dual, SOP-8) | Signal buffering and scaling, used on both the sensor-input side and the DAC-output side |
| LDO | AMS1117-3.3 (SOT-223) | 5V → 3.3V regulation |
| CAN transceiver | SN65HVD230DR-JSM | 3.3V-native CAN interface (chosen over TJA1050, which needs 5V) |
| Analog switch | **TS5A23157**DGSR (MSOP-10) | Hardware fail-safe, dual SPDT, defaults to OEM pass-through with zero power |
| AND gate | **SN74LVC1G08**DCKR (SOT-353) | Hardware-enforces both supervisor approval signals HIGH before boost can activate, cannot be bypassed by software |
| Output comparator | LM393DR(LX) (dual, SOIC-8) | Monitors MAIN+SUB output relationship, independent of firmware |
| Crystal | 8MHz SMD | Accurate clock for CAN bus timing (internal HSI's ±1% is too loose) |

The analog switch is TS5A23157, confirmed against the design study and the BOM.

## Safety architecture, Concept C: Safety-Partitioned Interposer

This is the most robust of three architectures considered in the requirements document:

- **Control path:** Main MCU calculates boost values, drives DAC outputs
- **Safety path:** Supervisor MCU independently monitors all signals and can force pass-through without depending on main MCU firmware
- **Hardware enforcement:** an AND gate (SN74LVC1G08) requires both supervisor-driven approval signals to be HIGH simultaneously before boost can activate, a two-key system that can't be triggered by a single stuck-at-HIGH fault, and keeps working even if all firmware crashes
- **Hardware default:** if the board has power but the MCU crashes or the firmware fails, the TS5A23157 analog switch returns to OEM pass-through with zero firmware involvement

> **Critical caveat, verified against the datasheet:** this fail-safe covers MCU/firmware failure while the board still has power. It does not cover total loss of board power. The TS5A23157's NC path requires VCC to conduct at all; with zero power, the switch goes to a high-impedance (open) state, not a closed one, no signal reaches the EPS at all. This holds for the whole switch family, including "powered-off protection" variants like the TS5A23159, whose protection means clean isolation when unpowered, not held connection. Mechanical relays and optocouplers were both investigated as alternatives that could genuinely provide zero-power pass-through; neither was found suitable (relay: footprint, switching speed, mechanical wear over vehicle service life; optocoupler: still requires power to pass a signal at all, and is poorly suited to continuous analog fidelity). This remains an open, unresolved gap, see [Validation Requirements](../open-items/README.md#critical-total-power-loss-is-not-currently-covered-by-any-fail-safe) for detail.

## Signal flow

![Signal flow diagram](../assets/diagrams/signal-flow.svg)

See [Safety Architecture](safety-architecture.md) for the full safety-goal-by-safety-goal breakdown, [Schematic Details](schematic-details.md) for the sheet-by-sheet signal path, and [Bill of Materials](bom.md) for the complete BOM.

## Gate control: split-pin design

Each chip drives its own independently-computed gate signal:

| Signal | Owner | Pin |
|---|---|---|
| `MCU_GATE_ENABLE` | Main MCU | PA8 |
| `SUPERVISOR_GATE_ENABLE` | Supervisor | PA6 |

Both feed the AND gate; `AND_OUT` drives the TS5A23157. Neither chip can single-handedly cause boost, each independently reasons about its own evidence and drives its own pin, and the AND gate only opens if both agree. This is a stronger safety property than the single-owner design: previously, a software bug in the supervisor's own decision logic could have driven both inputs HIGH together with nothing to catch it. Now, a bug would need to exist independently on **both** chips simultaneously to slip through. See [Firmware: Architecture](../firmware/architecture.md#split-pin-gate-control) for the full reasoning, and [Firmware: Main MCU](../firmware/main-mcu.md) / [Firmware: Supervisor MCU](../firmware/supervisor-mcu.md) for the per-chip pin tables.

## Design verification

Design assumptions were independently verified before being trusted:

- **LTspice simulation** confirmed the AFE and DAC-scaling op-amps must be powered from **VCC (5V)**, not the 3.3V rail, powering from 3.3V clips the output above ~3.1V, losing the upper half of the signal range. See [LTspice Simulation](ltspice-simulation.md).
- Architectural decisions were cross-checked against reference designs (e.g. TI2 / Torque Interceptor 2, Mazda SENT protocol) for lessons, not copied wholesale.

## Open hardware questions

Two questions remain open ahead of PCB layout, see [Validation Requirements](../open-items/README.md#requires-hardware-clarification).

## Source documents

The full original documents this section is derived from are included as reference in `assets/source-documents/`:

- `KommuAssist2_Hardware_Design_Study.docx`, the complete, authoritative hardware design study (REV 1.0, July 2026)
- `Safety-Rated_EPS_Torque_Sensor_Interface_PCB.pdf`, an earlier-stage version of the same rationale document

If anything here seems ambiguous or you need a detail not captured in these pages, check the original documents directly.
