# Shared Files

Code shared between the main MCU and supervisor MCU builds. Both projects use CubeMX's Basic application structure, so peripheral handles (`hadc1`, `huart1`/`huart2`, `hfdcan1`, `hiwdg`) are declared as plain globals directly in each project's `main.c`, with no `extern` declaration anywhere else. Both `main_mcu.c` and `main_supervisor.c` declare their own `extern` lines for the handles they need, matching `main.c` exactly.

## `fault_codes.h` / `fault_codes.c`

Defines the fault code enumeration and fault classification data used by both targets. Physically copied into both projects and kept identical on both sides, since a fault code sent over UART from one chip needs to mean the same thing on the other. This is where `fault_classify()` lives, see [Fault Handling](fault-handling.md#fault_classify) for its current status.

## `shared_protocol.h` / `shared_protocol.c`

Defines the UART frame protocol used between the main MCU and supervisor MCU: sync byte, sequence counter, checksum, and per-chip diagnostic fields (ADC readings, CAN value, fault codes). Kept identical on both sides.

This link is diagnostic and secondary, not part of the safety-critical gating decision, see [Firmware Architecture](architecture.md). Each chip computes and drives its own gate pin independently; UART carries cross-visibility information for logging and bench debugging.

The checksum is a simple additive byte sum, not a CRC, and is weak against certain corruption patterns. Upgrading it is recommended before a production revision.

## `reg_defs.h`

No longer in use. This was a hand-written register definition header from an earlier register-only approach, superseded by CubeMX-generated HAL code. Retained as documented history in [Firmware Architecture](architecture.md) for the register-offset bug it caught during development.
