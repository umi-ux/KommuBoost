# Hardware Summary

The hardware phase of this project is **complete**, formalized in Kommu's *KommuAssist2 — EPS Torque Sensor Interceptor PCB: Hardware Design Study* (REV 1.0, July 2026, CTO: Ting, ASIL B confirmed). That document is the authoritative source for hardware decisions — this page and its sub-pages summarize it for firmware engineers and for handover.

## Signal path

The board sits inline between the Perodua EPS torque sensor and the EPS ECU:

```
EPS ECU  <──[Interceptor PCB]──>  Torque Sensor
```

Default state (no power, or the analog switch not actuated): **OEM pass-through**, hardware-guaranteed, no firmware required. Any fault, uncertainty, timeout, power issue, software error, signal mismatch, or diagnostic failure returns the ECU to the original torque sensor signal without delay.

> **Prototype/study phase only.** Public-road deployment requires functional safety assessment, legal review, vehicle-level validation, and regulatory approval — not yet performed.

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
| Boost threshold | Value > 200 (units TBC — see [Open Items](../open-items/README.md#is-boost-threshold-200-in-scanner-units-or-raw-adc-value)) |
| Fault code C1513 | Signal mismatch (MAIN + SUB ≠ 5V) |
| Fault code C1515 | Signal stuck at 0 or 255 |

## Power architecture

The PCB is powered from the **ECU's 5V VDD pin**, not the 12V battery (confirmed by Ting):

- No 12V step-down converter needed — simpler design
- Cleaner power — already regulated by the ECU
- PCB taps a small amount of ECU 5V while passing the full 5V through to the sensor unchanged
- 5V regulated to 3.3V on-board via **AMS1117-3.3** LDO for all digital chips

## Key components

| Component | Part | Purpose |
|---|---|---|
| Main MCU | STM32G0B1CBT6 (LQFP-48) | Control path — calculates boost values, drives DAC outputs |
| Supervisor MCU | STM32G030F6P6TR (TSSOP-20) | Safety path — independently monitors signals, can force pass-through without relying on main MCU firmware |
| Op-amp (AFE + DAC scaling) | MCP6002DRG (dual, SOP-8) | Signal buffering and scaling — used on both the sensor-input side and the DAC-output side |
| LDO | AMS1117-3.3 (SOT-223) | 5V → 3.3V regulation |
| CAN transceiver | SN65HVD230DR-JSM | 3.3V-native CAN interface (chosen over TJA1050, which needs 5V) |
| Analog switch | **TS5A23157**DGSR (MSOP-10) | Hardware fail-safe — dual SPDT, defaults to OEM pass-through with zero power |
| AND gate | **SN74LVC1G08**DCKR (SOT-353) | Hardware-enforces both supervisor approval signals HIGH before boost can activate — cannot be bypassed by software |
| Output comparator | LM393DR(LX) (dual, SOIC-8) | Monitors MAIN+SUB output relationship, independent of firmware |
| Crystal | 8MHz SMD | Accurate clock for CAN bus timing (internal HSI's ±1% is too loose) |

> **Part number correction:** the analog switch is **TS5A23157**, not TS5A23159 as earlier notes had it. TS5A23157 is confirmed against the formal design study and is what's on the BOM.

## Safety architecture — Concept C: Safety-Partitioned Interposer

This is the most robust of three architectures considered in the requirements document:

- **Control path:** Main MCU calculates boost values, drives DAC outputs
- **Safety path:** Supervisor MCU independently monitors all signals and can force pass-through without depending on main MCU firmware
- **Hardware enforcement:** an AND gate (SN74LVC1G08) requires both supervisor-driven approval signals to be HIGH simultaneously before boost can activate — a two-key system that can't be triggered by a single stuck-at-HIGH fault, and keeps working even if all firmware crashes
- **Hardware default:** the TS5A23157 NC analog switch defaults to OEM pass-through with zero power and zero firmware involvement

See [Safety Architecture](safety-architecture.md) for the full safety-goal-by-safety-goal breakdown, [Schematic Details](schematic-details.md) for the sheet-by-sheet signal path, and [Bill of Materials](bom.md) for the complete BOM.

## Corrected: pin ownership of `FORCE_PT` / `GATE_ENABLE`

This was previously an open contradiction in this GitBook (scenario-flow doc said main MCU, schematic said supervisor). **The design study resolves it: the supervisor owns both.**

| Signal | Owner | Pin |
|---|---|---|
| `FORCE_PT` | **Supervisor** | PA6 |
| `GATE_ENABLE` | **Supervisor** | PA5 |

Both feed the AND gate; `AND_OUT` drives the TS5A23157. The main MCU does **not** own these pins — its earlier documented pin assignments (`FORCE_PT=PA6`, `GATE_ENABLE=PA5` on the *main* MCU) were incorrect and have been corrected throughout this book. See [Main MCU](../firmware/main-mcu.md) and [Supervisor MCU](../firmware/supervisor-mcu.md) for the corrected, per-chip pin tables.

## Design verification

Design assumptions were independently verified before being trusted:

- **LTspice simulation** confirmed the AFE and DAC-scaling op-amps must be powered from **VCC (5V)**, not the 3.3V rail — powering from 3.3V clips the output above ~3.1V, losing the upper half of the signal range. See [LTspice Simulation](ltspice-simulation.md).
- Architectural decisions were cross-checked against reference designs (e.g. TI2 / Torque Interceptor 2, Mazda SENT protocol) for lessons, not copied wholesale.

## Open hardware questions

Four questions are explicitly awaiting Ting's answer before PCB layout — see [Open Items](../open-items/README.md#hardware-open-questions-from-design-study).

## Source documents

The full original documents this section is derived from are included as reference in `assets/source-documents/`:

- `KommuAssist2_Hardware_Design_Study.docx` — the complete, authoritative hardware design study (REV 1.0, July 2026)
- `Safety-Rated_EPS_Torque_Sensor_Interface_PCB.pdf` — an earlier-stage version of the same rationale document

If anything here seems ambiguous or you need a detail not captured in these pages, check the original documents directly.
