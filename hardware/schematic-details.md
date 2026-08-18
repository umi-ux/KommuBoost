# Schematic Details

Sheet-by-sheet breakdown from the hardware design study. Designed in EasyEDA with the LCSC component library.

## Sheet overview

| Sheet | Title | Key components |
|---|---|---|
| Sheet 1 | Power Supply + CAN Interface + Debug | AMS1117-3.3, SN65HVD230, SWD headers |
| Sheet 2 (Output Submodule) | Output Submodule | MCP6002 ×2, TS5A23157, SN74LVC1G08, LM393 |
| AFE block | AFE & Conditioning | MCP6002 ×2, RC filters, voltage dividers |
| MCU block | Main MCU | STM32G0B1CBT6, 8MHz crystal |
| Supervisor block | Safety Supervisor | STM32G030F6P6TR |

All sheets are marked complete in the design study.

## Sheet 1, Power Supply

```
ECU 5V (VCC) → D1 Schottky (reverse polarity) → D2 TVS (load dump)
             → AMS1117-3.3 → +3.3V rail
             → C1 (10µF input cap) + C2 (22µF output cap)
             → R1 (330Ω) + LED2 (power indicator)
```

| Ref | Value | Package | Purpose |
|---|---|---|---|
| H1 | HDR-F-2.54 2×2 | Through-hole | ECU-side connector |
| H2 | HDR-F-2.54 2×2 | Through-hole | Sensor-side connector |
| U1 | AMS1117-3.3 | SOT-223 | 5V → 3.3V LDO regulator |
| C1 | 10µF | 0805 | LDO input decoupling |
| C2 | 22µF | 0805 | LDO output stability (mandatory per datasheet) |
| R1 | 330Ω | 0402 | LED current limiting |
| LED2 | LED | 0402 | Power indicator |
| D1 | SS14 Schottky | SOD-123 | Reverse polarity protection |
| D2 | SMBJ5.0A TVS | SMB | Load dump protection |

## AFE, Analog Front End & Conditioning

Signal flow per channel (×2, for MAIN and SUB):

```
Sensor signal (0–5V)
  → R_protect (100Ω) + D_clamp (3.6V zener)   [short-circuit + overvoltage protection]
  → R_bias (1MΩ to GND)                        [open-wire detection]
  → MCP6002 unity gain buffer                  [high-Z isolation]
  → RC filter (1kΩ + 100nF)                    [noise removal, cutoff ~1.6kHz]
  → Voltage divider (10kΩ/20kΩ)                [5V → 3.3V scaling]
  → MAIN_ADC / SUB_ADC → MCU PA0/PA1 + Supervisor PA0/PA1
```

| Ref | Value | Purpose |
|---|---|---|
| R_protect ×2 | 100Ω | Short-circuit protection on MAIN and SUB lines |
| D_clamp ×2 | BZX84C3V6 3.6V zener | Overvoltage clamp on MAIN and SUB |
| R_bias ×2 | 1MΩ | Open-wire pull-down, a 0V reading means a broken wire |
| U2.1, U2.2 | MCP6002 | Unity gain buffer, high-impedance sensor isolation |
| R2, R7 | 1kΩ | RC filter resistor per channel |
| C4, C5 | 100nF | RC filter capacitor per channel |
| R3, R6 | 10kΩ | Voltage divider, top resistor |
| R4, R5 | 20kΩ | Voltage divider, bottom resistor |

## Main MCU block, STM32G0B1CBT6

| Pin | Net label | Direction | Purpose |
|---|---|---|---|
| PA0 (11) | `MAIN_ADC` | In | Read conditioned torque MAIN signal |
| PA1 (12) | `SUB_ADC` | In | Read conditioned torque SUB signal |
| PA4 (15) | `DAC_MAIN` | Out | DAC channel 1, MAIN modified signal |
| PA5 (16) | `DAC_SUB` | Out | DAC channel 2, SUB modified signal |
| PB8 (47) | `CAN_RX` | In | FDCAN receive from vehicle bus |
| PB9 (48) | `CAN_TX` | Out | FDCAN transmit to vehicle bus |
| PC6 (30) | `HEARTBEAT_OUT` | Out | 50ms pulse to supervisor watchdog |
| PA9 (29) | `UART_TX` | Out | Diagnostic UART to supervisor |
| PA8 (28) | `UART_RX` | In | Diagnostic UART from supervisor |
| PA13 (35) | `MCU_SWDIO` | Debug | SWD programming data |
| PA14 (36) | `MCU_SWDCLK` | Debug | SWD programming clock + BOOT0 |
| PC14/PC15 (2/3) | Crystal X1 | Clock | 8MHz external crystal |
| VBAT (4) | +3.3V | Power | Backup domain supply |
| VREF+ (5) | +3.3V + 100nF | Power | ADC reference voltage |
| VDD/VDDA (6) | +3.3V | Power | Main and analog supply |

> **Note:** `HEARTBEAT_OUT` is `PC6`, not `PA10`. Earlier notes had this on `PA10`, corrected here against the design study.

## Supervisor block, STM32G030F6P6TR

| Pin | Net label | Direction | Purpose |
|---|---|---|---|
| PA0 (7) | `MAIN_ADC` | In | Independent torque MAIN read |
| PA1 (8) | `SUB_ADC` | In | Independent torque SUB read |
| PA2 (9) | `HEARTBEAT_IN` | In | Main MCU watchdog heartbeat |
| PA3 (10) | `UART_TX` | Out | Diagnostic to main MCU (crossover) |
| PA4 (11) | `UART_RX` | In | Diagnostic from main MCU (crossover) |
| PA5 (12) | `GATE_ENABLE` | Out | Arms the output stage (to AND gate) |
| PA6 (13) | `FORCE_PT` | Out | Active safety signal (to AND gate) |
| PA7 (14) | `5V_MON` | In | ECU 5V rail health monitoring |
| PB0 (15) | `FAULT_OUT` | In | Hardware output fault from LM393 |
| PA13 (18) | `SWDIO` | Debug | SWD programming data |
| PA14 (19) | `SWDCLK` | Debug | SWD programming clock + BOOT0 |
| VDDA/DDA (4) | +3.3V | Power | Combined VDD+VDDA (TSSOP-20 pinout) |

> The TSSOP-20 package combines VDD+VDDA onto one physical pin (Pin 4) and VSS+VSSA onto one pin (Pin 5). This is correct per the datasheet, not a schematic error.

> **This is the corrected ownership** for `FORCE_PT`/`GATE_ENABLE`, see [Hardware Summary](README.md#corrected-pin-ownership-of-force_pt--gate_enable).

## Sheet 2, Output Submodule

**Sub-circuit 1: DAC scaling** (MCP6002 U6.1 + U6.2)

```
DAC_MAIN (0–3.3V from MCU PA4)
  → R22 (1kΩ) + C19 (100nF)        [RC filter, removes DAC glitches]
  → U6.1 MCP6002 IN+ (Pin 3)
  → Gain = 1 + (R19/R20) = 1 + (10k/20k) = 1.5
  → DAC_MAIN_OUT (0–5V) → TS5A23157 NO1

DAC_SUB, identical circuit → DAC_SUB_OUT (0–5V) → TS5A23157 NO2
```

**Sub-circuit 2: Dual gate logic** (SN74LVC1G08 U9)

```
FORCE_PT (supervisor PA6)    → AND gate Pin A
GATE_ENABLE (supervisor PA5) → AND gate Pin B
                              → AND_OUT → TS5A23157 IN1 + IN2

AND_OUT = HIGH only when BOTH FORCE_PT=HIGH AND GATE_ENABLE=HIGH
```

**Sub-circuit 3: Analog switch** (TS5A23157 U7)

```
AND_OUT = LOW (default, pass-through):
  COM1 (10) ↔ NC1 (9): MAIN (OEM) → MAIN_OUT → ECU
  COM2 (6)  ↔ NC2 (7): SUB (OEM)  → SUB_OUT  → ECU

AND_OUT = HIGH (boost active):
  COM1 (10) ↔ NO1 (2): DAC_MAIN_OUT → MAIN_OUT → ECU
  COM2 (6)  ↔ NO2 (4): DAC_SUB_OUT  → SUB_OUT  → ECU
```

**Sub-circuit 4: Output monitor** (LM393 U8)

```
MAIN_OUT ── R26 (10kΩ) ──┐
                          ├── R31 (1kΩ) ──┬── LM393 Pin3 (1IN+)
SUB_OUT  ── R30 (10kΩ) ──┘                └── C24 (100nF) → GND

VREF (2.5V) ────────────────────────────────── LM393 Pin2 (1IN-)

If (MAIN+SUB)/2 ≠ 2.5V → FAULT_OUT goes LOW → Supervisor PB0
```

### Why resistor averaging, not individual channel monitoring

The original design monitored `MAIN_OUT` and `SUB_OUT` individually against 2.5V. This was wrong: during normal boost, MAIN can legitimately be anywhere from 1.0V to 4.0V, so comparing it alone against 2.5V would false-trigger at normal operating values. The correct invariant is MAIN + SUB = 5V, regardless of the individual split, so two equal resistors average MAIN and SUB, and the average sits at 2.5V precisely when the complementary relationship holds. Any fault that breaks that relationship shifts the average away from 2.5V, and the comparator catches it, independent of what either MCU believes is happening.

The RC filter (1kΩ + 100nF, cutoff ~1.6kHz) ahead of the comparator input filters fast noise spikes from the automotive environment (motor switching, ignition, CAN traffic) without masking real, sustained faults.

**Power note:** LM393 VCC is 5V (must sit above the 0–5V signals it compares), but its pull-up resistors go to +3.3V, because `FAULT_OUT` feeds a 3.3V-logic supervisor GPIO, 5V would damage it.

## Net label cross-reference

| Net | From | To | Purpose |
|---|---|---|---|
| `VCC` | H1 connector | AMS1117, dividers, op-amps | ECU 5V supply |
| `+3.3V` | AMS1117 output | All digital chips | Regulated 3.3V |
| `MAIN` | H2 sensor connector | AFE input, TS5A23157 COM1 | OEM MAIN torque signal |
| `SUB` | H2 sensor connector | AFE input, TS5A23157 COM2 | OEM SUB torque signal |
| `MAIN_ADC` | AFE voltage divider | MCU PA0, Supervisor PA0 | Scaled MAIN (0–3.3V) |
| `SUB_ADC` | AFE voltage divider | MCU PA1, Supervisor PA1 | Scaled SUB (0–3.3V) |
| `DAC_MAIN` | MCU PA4 | Sheet 5 op-amp U6.1 | Modified MAIN (0–3.3V) |
| `DAC_SUB` | MCU PA5 | Sheet 5 op-amp U6.2 | Modified SUB (0–3.3V) |
| `DAC_MAIN_OUT` | Op-amp U6.1 output | TS5A23157 NO1 | Scaled modified MAIN (0–5V) |
| `DAC_SUB_OUT` | Op-amp U6.2 output | TS5A23157 NO2 | Scaled modified SUB (0–5V) |
| `AND_OUT` | SN74LVC1G08 output | TS5A23157 IN1, IN2 | Combined supervisor approval |
| `FORCE_PT` | Supervisor PA6 | AND gate Pin A | Active safety signal |
| `GATE_ENABLE` | Supervisor PA5 | AND gate Pin B | Output stage arm signal |
| `MAIN_OUT` | TS5A23157 NC1/NO1 | ECU via H1, LM393 IN+ | Final MAIN signal to ECU |
| `SUB_OUT` | TS5A23157 NC2/NO2 | ECU via H1, LM393 IN+ | Final SUB signal to ECU |
| `FAULT_OUT` | LM393 1OUT (Pin 1) | Supervisor PB0 (Pin 15) | Hardware output fault signal |
| `VREF` | R27+R28 divider | LM393 Pin 2 (1IN-) | 2.5V comparator reference |
| `5V_MON` | R13+R14 divider | Supervisor PA7 | ECU 5V health monitor (2.5V nominal) |
| `HEARTBEAT_OUT` | MCU PC6 | Supervisor PA2 | 50ms watchdog pulse |
| `UART_TX` / `UART_RX` | MCU PA9/PA8, Supervisor PA3/PA4 | Cross-connected | UART diagnostic link |
| `CAN_TX` / `CAN_RX` | MCU PB9/PB8 | SN65HVD230 | CAN bus |
