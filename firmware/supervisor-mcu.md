# Supervisor MCU (`STM32G030F8P6TR`)

Files: `main_supervisor.h`, `main_supervisor.c`

> **Part number correction:** the schematic dated 2026-08-17 shows `STM32G030F8P6TR` (64KB flash), not `STM32G030F6P6TR` (32KB flash) as earlier notes had it. Flagged in [Open Items](../open-items/README.md#pin-table-corrected-against-2026-08-17-schematic) to confirm this is intentional and not a part-selection typo.

## Role

The supervisor is deliberately the simpler, less busy chip. It independently monitors the system and drives its own half of the boost-approval gate (`SUPERVISOR_GATE_ENABLE`) — the main MCU drives the other half (`MCU_GATE_ENABLE`), and both must be HIGH for boost to activate. Neither chip can force boost alone.

This split is a deliberate ASIL B safety mechanism: the CAN-facing, functionally busy main MCU is not trusted with sole authority over whether boost is engaged. See [Key Learnings](../key-learnings/README.md#supervisor-holds-veto-is-correct-for-asil-b).

## Confirmed pin assignments

**Source: `Schematic_Torque-Interceptor_2026-08-17.png`, Safety Supervisor sheet.** This supersedes earlier pin tables in this book — several pins changed since the last revision (noted below).

| Pin | Net label | Direction | Purpose |
|---|---|---|---|
| PA0 (7) | `MAIN_ADC` | In | Independent torque MAIN read |
| PA1 (8) | `SUB_ADC` | In | Independent torque SUB read |
| PA2 (9) | `UART_TX` | Out | Diagnostic to main MCU. **Changed from `PA3`.** |
| PA3 (10) | `UART_RX` | In | Diagnostic from main MCU. **Changed from `PA4`.** |
| PA5 (12) | `HEARTBEAT` | In | Main MCU watchdog heartbeat. **Changed from `PA2`** (previously named `HEARTBEAT_IN`). |
| PA6 (13) | `SUPERVISOR_GATE_ENABLE` | Out | Supervisor's own half of the two-key AND-gate approval. **Replaces the old `FORCE_PT`/`GATE_ENABLE` pair** — see note below. |
| PA12 (17, remapped as PA10) | `FAULT_OUT` | In | Hardware output fault from LM393. **Changed from `PB0`.** |
| PA13 (18) | `SWDIO` | Debug | SWD programming data |
| ~PA14/19 | `SWDCLK` | Debug | SWD programming clock + BOOT0 |

> **`FORCE_PT`/`GATE_ENABLE` no longer exist as separate pins.** The current schematic shows a single `SUPERVISOR_GATE_ENABLE` pin (PA6) feeding one input of the AND gate, with `MCU_GATE_ENABLE` from the main MCU feeding the other (see [Main MCU](main-mcu.md)). This is a **different arrangement** than what this book previously documented (supervisor alone driving both AND-gate inputs) — it's a stronger two-key design, since now neither chip alone can assert both inputs. The [Hardware Summary](../hardware/README.md) and [Open Items](../open-items/README.md#pin-table-corrected-against-2026-08-17-schematic) reflect this correction; treat any earlier reference to `FORCE_PT` in this book as historical.
>
> **`PA7` (`5V_MON` in earlier notes) shows no connection on the current schematic.** ECU 5V rail monitoring may have been dropped in this revision, or the wire just isn't clearly visible in the source image — flagged in [Open Items](../open-items/README.md#5v_mon-rail-monitoring-appears-missing) to confirm with Ting rather than assumed either way.

## Timing

Like the main MCU, uses SysTick-based fixed-period timing to guarantee debounce and timeout cycles run predictably.

## Register access

Also uses the shared hand-written `reg_defs.h`, verified against RM0454 for this chip's peripheral set as well — register offsets are not automatically identical between the STM32G0B1 and STM32G030 and must be independently checked.

## Known gaps specific to the supervisor

- **Recovery-latch enforcement is not yet implemented on the supervisor.** The two-bucket fault recovery policy (see [Fault Handling](fault-handling.md)) is currently only enforced on the main MCU. For the supervisor's independence to be a meaningful safety mechanism, it likely needs its own enforcement of this policy rather than trusting the main MCU's bookkeeping.
- **No power rail self-check.** An identified architectural gap — the supervisor does not currently self-check its own power rail.

## UART link to main MCU

The supervisor communicates with the main MCU over UART. The frame format has been drafted but is **not yet confirmed with Ting** — this is the largest remaining chunk of firmware work. See [Open Items](../open-items/README.md#uart-protocol-between-mcus) and [Shared Files](shared-files.md#shared_protocolhc).
