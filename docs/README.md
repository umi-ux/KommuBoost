# Torque Interceptor, Project Documentation

This documentation covers the design, architecture, and current implementation status of the safety-rated torque interceptor PCB developed for Kommu's KommuAssist2 (bukapilot/openpilot fork).

**Status as of this handover:** Hardware phase complete. Firmware in active development.

## What this device does

The torque interceptor is a PCB that sits inline between a Perodua vehicle's EPS (Electric Power Steering) torque sensor and the EPS ECU. It allows KommuAssist2 to boost the steering assist signal by commanding the interceptor over CAN, while preserving a hardware-guaranteed pass-through path if anything goes wrong.

Target: **ASIL B** per ISO 26262.

## How to read this book

- **Overview**, what the system does, top-level goals, timeline context
- **Hardware**, board architecture, key components, signal path
- **Firmware**, the three-target firmware architecture (main MCU, supervisor MCU, shared code), state machine, and fault handling in detail
- **Open Items**, everything still unresolved, blocked, or pending sign-off, read this first if you're picking up the project
- **Key Learnings**, design principles and lessons learned that should inform any further work
- **Appendix**, tools, reference documents, pin assignments, and constants

## Quick orientation for a new reader

1. Start with [Project Overview](overview/README.md)
2. Read [Open Items & TODOs](open-items/README.md) to understand what's blocking progress
3. Then go deep into [Firmware Architecture](firmware/architecture.md)
