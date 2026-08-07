# Supervisor MCU (`STM32G030F6P6TR`)

Files: `main_supervisor.h`, `main_supervisor.c`

## Role

The supervisor is deliberately the simpler, less busy chip — and holds **final veto authority** over pass-through vs. boost via `FORCE_PT` and `GATE_ENABLE`. It independently monitors the system rather than trusting the main MCU's state.

This split is a deliberate ASIL B safety mechanism: the CAN-facing, functionally busy main MCU is not trusted with the final word on whether boost is engaged. See [Key Learnings](../key-learnings/README.md#supervisor-holds-veto-is-correct-for-asil-b).

## Confirmed pin assignments

| Pin | Net label | Direction | Purpose |
|---|---|---|---|
| PA0 | `MAIN_ADC` | In | Independent torque MAIN read |
| PA1 | `SUB_ADC` | In | Independent torque SUB read |
| PA2 | `HEARTBEAT_IN` | In | Main MCU watchdog heartbeat |
| PA3 | `UART_TX` | Out | Diagnostic to main MCU (crossover) |
| PA4 | `UART_RX` | In | Diagnostic from main MCU (crossover) |
| PA5 | `GATE_ENABLE` | Out | Arms the output stage (to AND gate) |
| PA6 | `FORCE_PT` | Out | Active safety signal (to AND gate) |
| PA7 | `5V_MON` | In | ECU 5V rail health monitoring |
| PB0 | `FAULT_OUT` | In | Hardware output fault from LM393 |
| PA13 | `SWDIO` | Debug | SWD programming data |
| PA14 | `SWDCLK` | Debug | SWD programming clock + BOOT0 |

**This ownership is confirmed**, resolving what was previously an open contradiction: the scenario flow document had described the *main* MCU raising `FORCE_PT`, while the schematic showed the *supervisor* owning it. Kommu's hardware design study settles this — the supervisor owns both `FORCE_PT` and `GATE_ENABLE`, feeding a hardware AND gate (SN74LVC1G08) whose output (`AND_OUT`) drives the TS5A23157 analog switch. Both signals must be HIGH simultaneously for boost to activate; the gate can't be bypassed by software on either chip. See [Hardware Summary](../hardware/README.md#corrected-pin-ownership-of-force_pt--gate_enable) and [Schematic Details](../hardware/schematic-details.md#sheet-2--output-submodule).

## Timing

Like the main MCU, uses SysTick-based fixed-period timing to guarantee debounce and timeout cycles run predictably.

## Register access

Also uses the shared hand-written `reg_defs.h`, verified against RM0454 for this chip's peripheral set as well — register offsets are not automatically identical between the STM32G0B1 and STM32G030 and must be independently checked.

## Known gaps specific to the supervisor

- **Recovery-latch enforcement is not yet implemented on the supervisor.** The two-bucket fault recovery policy (see [Fault Handling](fault-handling.md)) is currently only enforced on the main MCU. For the supervisor's independence to be a meaningful safety mechanism, it likely needs its own enforcement of this policy rather than trusting the main MCU's bookkeeping.
- **No power rail self-check.** An identified architectural gap — the supervisor does not currently self-check its own power rail.

## UART link to main MCU

The supervisor communicates with the main MCU over UART. The frame format has been drafted but is **not yet confirmed with Ting** — this is the largest remaining chunk of firmware work. See [Open Items](../open-items/README.md#uart-protocol-between-mcus) and [Shared Files](shared-files.md#shared_protocolhc).
