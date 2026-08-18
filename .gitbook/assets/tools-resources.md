# Tools & Resources

## MCUs

- **Main:** STM32G0B1CBT6
- **Supervisor:** STM32G030F6P6TR

## Reference documents

- **RM0454**, STM32G0 reference manual. The authoritative source for register offsets. Every register/offset in `reg_defs.h` must be checked against this before use.

## Schematic / PCB

- EasyEDA (design)
- LCSC / JLCPCB (sourcing and fabrication)

## Simulation

- LTspice, used to independently verify design assumptions (e.g. AFE gain) before relying on them in firmware.

## Firmware approach

- Direct register writes, no HAL, no CubeMX, no CMSIS headers
- Custom hand-written `reg_defs.h`, covering only the peripherals actually used

## Key components

| Component | Part |
|---|---|
| Dual op-amp (AFE + DAC scaling) | MCP6002DRG |
| LDO | AMS1117-3.3 |
| NC analog switch | TS5A23157DGSR |
| AND gate (hardware-enforced dual approval) | SN74LVC1G08DCKR |
| Comparator | LM393DR(LX) |
| CAN transceiver | SN65HVD230DR-JSM |

Full BOM with pricing: [Bill of Materials](../hardware/bom.md).

## Collaborators

- **Senior hardware engineer**, provides design requirements and sign-off on safety-relevant decisions (fault classification, UART protocol, boost formula, etc.)
