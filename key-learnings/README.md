# Key Learnings & Principles

Design principles that should inform any further work on this project, and the reasoning behind them.

## Hardware fail-safe superiority

An NC (normally closed) analog switch providing pass-through on power loss is architecturally stronger than a purely software-managed bypass, because it doesn't depend on firmware being alive at all. This is a core safety argument for the design, the system's most basic safe state doesn't require any software to be functioning correctly.

## Supervisor holds veto, correct for ASIL B

The busier, CAN-facing chip (main MCU) should not hold final safety authority. Giving the supervisor, simpler, less busy, independently monitoring, the final veto over `FORCE_PT`/`GATE_ENABLE` is the actual safety mechanism, not a redundant nicety. If the main MCU's software has a bug, the supervisor's independence is what catches it.

## Datasheet vs. reference manual

Datasheets give pinout and electrical specs. Reference manuals (e.g. RM0454) give the register-level detail needed for direct-register firmware. **Never invent a register**, verify every offset against the RM table before use. This rule exists because of a real bug: an invented RCC register (`CRRCR`) shifted `IOPENR`'s offset incorrectly, and was only caught by checking RM0454 Table 28 before flashing.

## ISO 26262 fault recovery framing

Once a safe state is reached instantly (satisfying FTTI), recovery *timing* becomes an availability question, not a safety question. This reframing is what justifies the two-bucket fault recovery policy, the policy is defended against the ASIL B latent fault detection metric (≥60%), not against prescriptive standard text. Useful to remember if the policy is ever challenged in review: the answer isn't "this is standard practice," it's "this meets the latent fault metric."

## Always-on validation, gated computation

Input validation and safety monitoring run continuously regardless of state. Boost computation only engages once CAN value > 200. This is the correct architectural split between the **safety monitoring function** (must always run) and the **ADAS function** (only runs when commanded and conditions are valid).

## Generated code requires skeptical review

Two real bugs were found in previously generated firmware before being trusted:

1. A shared debounce counter mixing unrelated fault types.
2. Escalation logic forgetting earlier compute-integrity faults within the same drive cycle.

Neither was caught by tests, both were caught by deliberate, skeptical line-by-line review against the intended policy. **Verification against authoritative sources (schematic, RM tables, the actual safety policy) is essential before flashing anything, especially generated code.**

## AFE gain must be accounted for

Assuming no gain in the analog front-end leads to incorrect ADC valid-range bounds, and downstream, either false faults or missed detections. The confirmed 0.645 gain from the 10k/20k divider was folded into the ADC constants for exactly this reason, see [Main MCU](../firmware/main-mcu.md#adc-constants-afe-derived).
