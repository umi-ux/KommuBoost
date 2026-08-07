# Shared Files

Code shared between the main MCU and supervisor MCU builds.

## `fault_codes.h` / `fault_codes.c`

Defines the fault code enumeration and fault classification data used by both targets. This is where `fault_classify()` lives — see [Fault Handling](fault-handling.md#fault_classify--status) for its current status (3 of 10 entries confirmed, 7 pending Ting's FMEDA review).

## `shared_protocol.h` / `shared_protocol.c`

Defines the UART frame protocol used between the main MCU and supervisor MCU.

**Status: drafted but unconfirmed with Ting.** This is currently the largest unresolved piece of firmware work in the project — see [Open Items](../open-items/README.md#uart-protocol-between-mcus). Nothing downstream of this (e.g. supervisor-side recovery-latch enforcement, which needs to communicate fault/recovery state to the main MCU) can be finalized until the frame format is signed off.

## `reg_defs.h`

Technically shared infrastructure rather than a "shared file" in the same sense — a hand-written, minimal register definition header covering only the GPIO and RCC peripherals actually used by either target. See [Firmware Architecture](architecture.md#no-hal--register-level-approach) for the rationale and the bug history behind the rule to never invent a register or offset.
