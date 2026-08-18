# KommuBoost

Firmware for the Torque Interceptor PCB, a safety-rated inline device that sits between the Perodua EPS torque sensor and the EPS ECU, allowing KommuAssist2 to boost steering assist via CAN command.

Full documentation, architecture, diagrams, and open items: **https://kommuboost.gitbook.io/kommuboost**

## What this repo contains

Firmware source for both chips on the board. Each folder is a separate STM32CubeIDE project, built independently, flashed onto its own chip.

```
KommuBoost/
├── Main MCU/           STM32G0B1CBT6 firmware
└── Safety Supervisor/  STM32G030F6P6TR firmware
```

## Architecture, in short

Two independent chips, each computing its own decision and driving its own gate signal into a shared AND gate:

- **Main MCU** reads the torque sensor, listens for the boost command over CAN, computes the boost value, and drives `MCU_GATE_ENABLE` from its own logic.
- **Safety Supervisor** independently reads the same sensor line on its own ADC taps, watches the main MCU's heartbeat and the hardware fault comparator, and drives `SUPERVISOR_GATE_ENABLE` from its own logic.

Boost only reaches the EPS if both chips independently agree. Neither chip can activate it alone. Full reasoning and diagrams are in the GitBook.

## Building

Each project is a standard STM32CubeIDE project (CubeMX-generated, HAL-based):

1. Open STM32CubeIDE
2. Import → Existing Projects into Workspace
3. Select `Main MCU` or `Safety Supervisor`
4. Build

Both projects share `fault_codes.h/.c` and `shared_protocol.h/.c`, the common fault vocabulary and UART diagnostic frame format. These are duplicated in each project folder and must be kept identical between the two.

## Status

Firmware is written and compiles clean against real hardware headers on both chips. Several items still require review or bench hardware before this is production-ready, most notably:

- A safety-critical hardware gap: total loss of board power is not currently covered by any fail-safe mechanism (verified against the analog switch's datasheet, see the GitBook for detail)
- Fault classification table: 5 of 12 fault types confirmed, 7 pending FMEDA review
- CAN signal decoding needs validation against a live captured frame
- Boost formula and all timing constants are placeholders pending bench testing

Full list: see Validation Requirements in the GitBook.

## Safety notice

Prototype and study phase only. Not validated for public-road deployment. Requires functional safety assessment, legal review, vehicle-level validation, and regulatory approval before use in a vehicle.
