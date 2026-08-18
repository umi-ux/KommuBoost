# Project Overview

## Context

Kommu is a Malaysian ADAS company building KommuAssist2, a fork of openpilot/bukapilot. The torque interceptor is a safety-rated PCB that enables KommuAssist2 to actively boost steering assist on Perodua vehicles.

This project was developed under the supervision of a senior hardware engineer, who provided design requirements and final sign-off on safety-relevant decisions.

## What problem it solves

Perodua's EPS system has its own torque sensor and ECU. To let KommuAssist2 influence steering assist, something needs to sit between the torque sensor and the ECU and be able to:

- Pass the torque sensor signal through unmodified during normal operation
- Boost the signal on command from KommuAssist2 over CAN, when appropriate
- Guarantee it never gets "stuck" boosting or feeding a corrupted signal, with hardware-level guarantees, not just firmware

## Target safety level

**ASIL B** per ISO 26262. This shapes almost every design decision in the project:

- Dual-MCU architecture (main + supervisor) so no single point of software failure has final authority
- Hardware fail-safe (NC analog switch) as the ultimate backstop, independent of firmware being alive
- Fault classification and recovery policy justified against the ASIL B latent fault detection metric (≥60%), not just "best practice"

## Timeline

Roughly a two-month prototype timeline, targeting JLCPCB fabrication and PCBA assembly. Hardware phase is complete; the project is now in firmware development.

## High-level architecture

Two MCUs on the board:

| MCU | Part | Role |
|---|---|---|
| Main MCU | STM32G0B1CBT6 | CAN-facing, owns the state machine, drives the DAC output |
| Supervisor MCU | STM32G030F6P6TR | Holds physical veto authority (`FORCE_PT`/`GATE_ENABLE`), independent monitoring |

Key idea: the busier, CAN-facing chip (main MCU) does **not** hold final safety authority. The supervisor, simpler, less busy, independently monitoring, holds the veto. This separation is itself a safety mechanism appropriate for ASIL B.

On top of that, a hardware-level fail-safe (an NC, normally closed, analog switch) provides pass-through by default on power loss, with no dependency on any firmware being alive at all. This is architecturally stronger than a purely software-managed bypass and is a core safety argument for the whole design, see [Key Learnings](../key-learnings/README.md) for more on why.
