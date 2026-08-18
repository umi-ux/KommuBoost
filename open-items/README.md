# Validation Requirements

Items required before this design is treated as production-ready, grouped by what resolves them.

## Critical: total power loss is not currently covered by any fail-safe

This is the highest-priority open item in the project. The design's hardware fail-safe (the TS5A23157 analog switch defaulting to OEM pass-through) has been described in earlier project documentation as covering "zero power," which overstates what the hardware actually guarantees.

**Verified against the TI datasheet:** the switch's NC pass-through behavior requires VCC to be present, the internal FET conduction path needs bias voltage to hold any state, including the default one. With zero board power, the switch goes high-impedance (open), not closed. This means: if the board loses power entirely, no signal, not raw, not boosted, reaches the EPS.

**What is genuinely covered:** MCU or firmware failure while the board still has power (crash, hang, firmware bug, brownout with power still present). In that case both gate pins fall LOW and the switch correctly returns to pass-through with zero firmware involvement. This remains a real, verified safety property, just narrower than "handles zero power."

**Investigated and ruled out:**
- A "powered-off protection" switch variant (TS5A23159) does not solve this. That feature means clean isolation when unpowered, not held connection, every switch in this family shares the same underlying limitation.
- A mechanical relay could genuinely hold a zero-power NC contact, but was ruled out for this application: footprint too large, switching speed too slow, mechanical wear over the vehicle's service life, and higher coil-drive current.
- An optocoupler does not solve the problem at all, it still requires power to pass a signal, and is poorly suited to continuous analog voltage fidelity.

**This needs a decision, not just an acknowledgment.** Options include: accepting the narrower safety claim as sufficient for the target ASIL, finding a different fail-safe mechanism for total power loss specifically, or determining that total power loss is an acceptable residual risk given the vehicle's own behavior under total EPS power loss (worth checking what the OEM EPS itself does if its own power is cut, since that may already be a scenario the vehicle handles independently of this board). This is a decision for the FMEDA, not something to resolve unilaterally.

## Requires FMEDA / design review

### `fault_classify()` table

5 of 12 fault types are confirmed (see [Fault Handling](../firmware/fault-handling.md#fault_classify) for the full list). The remaining 7 are placed conservatively pending FMEDA review. `DRIVER_OVERRIDE` is the strongest candidate for reclassification: it reflects normal driving behavior, not a malfunction, and its current strict classification risks an unnecessary ignition-cycle-required lockout after ordinary driver overrides. See [Fault Handling](../firmware/fault-handling.md#fault_classify).

### `STEER_CMD` bit-extraction

Implemented per the DBC's documented bit layout and internally self-consistent, but not yet checked against a live captured CAN frame with a known value. This requires a real reference data point, not a re-read of the specification text. See [Main MCU](../firmware/main-mcu.md#can-message).

### Rail-voltage sense pin (supervisor)

Planned as a supervisor-side self-check, not wired into the schematic or CubeMX pin configuration. Requires a decision on whether to add the wiring or drop the requirement.

## Requires bench hardware

### Tunable constants

Debounce sample counts, timeout windows, hysteresis gap, and loop period are all structurally sound but not bench-validated. Requires systematic validation once a fabricated board exists, using the ESP32 bench-test rig (fake torque sensor and CAN injector).

### Boost formula tuning

The formula structure (`dac_main = main_adc + BOOST_OFFSET_COUNTS`, `dac_sub` derived to preserve the sum invariant) is in place; `BOOST_OFFSET_COUNTS` is an unvalidated placeholder. Requires bench testing to determine a real value, and confirmation that a flat offset is the correct formula shape.

## Requires hardware clarification

Two questions remain open, needed before PCB layout proceeds:

- **Maximum safe current draw from the ECU's 5V VDD.** Needed to confirm the PCB's power budget will not disturb the sensor supply or trigger an ECU fault.
- **Whether the signal correlation check needs an additional hardware comparator**, beyond the existing LM393 resistor-averaging circuit on the output side, potentially at the ADC input stage.

## Design decisions closed

- **Gate control architecture.** Each chip drives its own gate signal (`MCU_GATE_ENABLE`, `SUPERVISOR_GATE_ENABLE`) independently into a shared AND gate. This supersedes an earlier single-owner design where the supervisor alone drove both signals. See [Firmware Architecture](../firmware/architecture.md).
- **CAN message.** `BO_ 464 STEERING_LKAS` (`STEER_REQ` + `STEER_CMD`) is the confirmed message, replacing an earlier unconfirmed candidate that had no request bit.
- **Boost threshold.** `STEER_CMD` compared against 200, raw field value, with a 15-count hysteresis band on exit.
- **ADC voltage reference.** `VREF+` ties directly to the 3.3V rail, verified against the schematic.
- **CAN transceiver wiring.** SN65HVD230DR-JSM wiring checked pin-by-pin against its datasheet application circuit.
- **Debounce and escalation logic.** Both identified bugs, a shared debounce counter mixing fault types, and escalation forgetting earlier faults within a drive, are fixed on both chips.
- **`SUB_ADC` stuck-check.** Both signals are independently checked on both chips.
- **Watchdog.** IWDG active on both chips, with reset-cause detection reporting `FAULT_WATCHDOG_RESET` after a hang-triggered reset.

## Deferred, not blocking

- **Fault logging.** No-op on both chips. Acceptable until bench testing begins.
- **UART checksum.** Simple additive sum, not a CRC. Upgrade recommended before production, not urgent for prototype bring-up.
- **Ignition-cycle detection.** Likely unnecessary given the board's power architecture (loss of power is a natural reset that clears fault memory regardless), pending one explicit confirmation that no always-on supply path exists.
- **Supervisor fault memory.** Currently stateless by design. Whether it should adopt any form of fault memory is a deliberate design question, not an assumed gap.

## Priority summary

| Item | Blocks | Requires |
|---|---|---|
| `fault_classify()` table | Fault handling confidence, FMEDA sign-off | Design review |
| `STEER_CMD` bit-extraction validation | Trusting the boost trigger | Live reference frame |
| Boost formula tuning | Real boost testing | Bench hardware |
| Tunable constants | Timing confidence | Bench hardware |
