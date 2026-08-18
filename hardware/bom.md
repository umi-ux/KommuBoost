# Bill of Materials & Gap Analysis

## Complete BOM

| Ref | Component | Value/Part | Package | Qty | LCSC est. price |
|---|---|---|---|---|---|
| U1 | LDO Regulator | AMS1117-3.3 | SOT-223 | 1 | $0.10–0.20 |
| U2 | Op-amp (AFE) | MCP6002DRG | SOP-8 | 1 | $0.20–0.40 |
| U3 | Main MCU | STM32G0B1CBT6 | LQFP-48 | 1 | $2.22 |
| U4 | CAN Transceiver | SN65HVD230DR | SOIC-8 | 1 | $0.30–0.50 |
| U5 | Safety Supervisor | STM32G030F6P6TR | TSSOP-20 | 1 | $0.45 |
| U6 | Op-amp (Output) | MCP6002DRG | SOP-8 | 1 | $0.20–0.40 |
| U7 | Analog Switch | TS5A23157DGSR | MSOP-10 | 1 | $0.20–0.30 |
| U8 | Comparator | LM393DR(LX) | SOIC-8 | 1 | $0.05–0.10 |
| U9 | AND Gate | SN74LVC1G08DCKR | SOT-353 | 1 | $0.05–0.10 |
| X1 | Crystal | 8MHz SMD | SMD-3225 | 1 | $0.10–0.20 |
| D1 | Schottky Diode | SS14 | SOD-123 | 1 | $0.05 |
| D2 | TVS Diode | SMBJ5.0A | SMB | 1 | $0.10 |
| D_clamp ×2 | Zener Diode | BZX84C3V6 | SOT-23 | 2 | $0.05 each |
| D_ESD_CAN | CAN ESD | PESD1CAN | SOT-23 | 1 | $0.15 |
| LED2 | LED | Any green | 0402 | 1 | $0.02 |
| H1, H2 | Connector | HDR-F-2.54 2×2 | Through-hole | 2 | $0.10 each |
| H3 | Connector | HDR-F-2.54 1×2 | Through-hole | 1 | $0.05 |
| MCU_HEADER1, SUP_HEADER1 | SWD Header | HDR-M-2.54 1×3 | Through-hole | 2 | $0.05 each |
| Capacitors (all) | Various | 100nF, 1µF, 4.7µF, 10µF, 18pF, 22µF | 0402/0805 | ~20 | ~$0.30 total |
| Resistors (all) | Various | 100Ω, 330Ω, 1kΩ, 10kΩ, 20kΩ, 1MΩ | 0402 | ~30 | ~$0.30 total |

**Estimated total BOM cost (single prototype unit): ~$5.50 – $7.00**

## Gaps identified and addressed

These were gaps against the requirements document that were caught and closed during this design study:

| Gap | Requirement ref | Resolution | Sheet |
|---|---|---|---|
| Reverse polarity protection | §7.6 | Added SS14 Schottky diode (D1) | Sheet 1 |
| Load dump protection | §7.6 | Added SMBJ5.0A TVS diode (D2) | Sheet 1 |
| Short-circuit protection on signal lines | §7.3 | Added 100Ω series resistors on MAIN and SUB | AFE |
| Overvoltage clamp on signal lines | §7.3 | Added BZX84C3V6 3.6V zener on MAIN and SUB | AFE |
| Open-wire detection | §7.3 | Added 1MΩ pull-down on MAIN and SUB lines | AFE |
| ESD protection on CAN connector | §7.7 | Added PESD1CAN on CANH and CANL at H3 | Sheet 6 (CAN) |
| Output monitor false-triggering | §7.2 | Replaced individual-channel monitoring with resistor-averaging circuit | Output Submodule |
| Noise on output monitor | §7.3 | Added RC filter (1kΩ + 100nF) before comparator input | Output Submodule |

## Known remaining gaps (accepted for prototype stage)

| Gap | Requirement ref | Status | Mitigation |
|---|---|---|---|
| AEC-Q component qualification | §7.1 | Partially addressed | Industrial-grade components used; automotive variants to be evaluated for production |
| Separate sensor VDD monitoring | §7.2 | Accepted simplification | `5V_MON` covers the same rail; dedicated monitoring planned for V2 |
| ESD protection on H1/H2 signal connectors | §7.7 | Partial | PESD1CAN added to CAN; H1/H2 ESD arrays planned for V2 |
| Cranking/brownout behavior | §7.6 | Partial | Supervisor `5V_MON` detects undervoltage; AMS1117 handles brownout |
| Full FMEDA/FTA analysis | §13 | Not yet done | Required before moving from prototype to production |
| Cybersecurity threat analysis | §10 | Not yet done | Required before any vehicle deployment |
| Formal HARA | §4 | Not yet done | Required before any vehicle deployment |

These are accepted for the prototype/study phase specifically, none of them are acceptable gaps for a production or public-road-deployed unit. See the note on scope at the top of [Hardware Summary](README.md).

## Next steps (from the design study)

**Immediate, before PCB layout:**
- Resolve the [3 open hardware questions](../open-items/README.md#requires-hardware-clarification)
- Add the protection components to the schematic (D1, D2, D_clamp, R_protect, R_bias, PESD1CAN) if not already placed
- Run ERC (Electrical Rules Check) in EasyEDA and fix all errors
- Final schematic review

**PCB layout phase:**
- Analog/digital partitioning, keep analog signal traces away from digital switching
- Decoupling caps placed as close as possible to each chip's power pin
- CAN bus layout rules, controlled impedance, matched-length CANH/CANL
- Single solid ground plane
- Keep MAIN and SUB analog traces away from CAN bus traces
- Test points on all key nets for bench debugging

**Validation stages (per requirements document §12):**

| Stage | Activity | Gate criteria |
|---|---|---|
| 1 | Bench signal characterization | Measure actual Perodua torque sensor signals |
| 2 | Signal emulator bench test | Verify pass-through, no-glitch switching, fault detection |
| 3 | Hardware-in-the-loop | Simulate CAN, sensor faults, power faults |
| 4 | Vehicle static test (wheels lifted) | Verify EPS behavior, fail-safe, power cycle |
| 5 | Closed-track test | Only after independent safety review |

> No public-road testing during development. No deployment without independent safety assessment (requirements document §15).
