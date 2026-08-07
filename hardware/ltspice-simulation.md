# LTspice Simulation — MCP6002 AFE

## Objective

Verify the MCP6002 op-amp circuit behavior on the AFE (sensor input) and DAC-scaling (output) stages before trusting the design — specifically to check signal range and identify clipping.

## AFE circuit — unity gain buffer

**Test setup:** 0–5V sinusoidal input (representing torque sensor MAIN output), MCP6002 as a unity-gain buffer (IN- tied to OUT), RC filter + voltage divider load.

**Findings:**

- At VCC = 5V: output correctly follows input across the full 0–5V range ✅
- At VCC = 3.3V: output clips at ~3.1V — the upper portion of the signal (3.1V–5V) is lost ❌

**Design decision:** the MCP6002's VDD pin (Pin 8) is connected to VCC (5V), not the 3.3V rail, on both the AFE and DAC-scaling stages. This gives rail-to-rail output across the full 0–5V torque signal range. The 3.3V rail is reserved for digital chips only (main MCU, supervisor, AND gate).

## DAC scaling circuit — non-inverting amplifier

**Test setup:** 0–3.3V sinusoidal input (representing the MCU DAC output), MCP6002 as a non-inverting amplifier, Rf = 10kΩ / Rg = 20kΩ (theoretical gain 1.5), VCC = 5V.

**Findings:**

- Output range: 0V to ~4.95V (expected 5V — within component tolerance) ✅
- Gain verified: 3.3V × 1.5 = 4.95V, matching the theoretical calculation ✅
- No clipping across the full input range ✅
- Signal shape preserved, minimal distortion ✅

> LTspice simulation screenshots are embedded directly in the EasyEDA schematic (Sheet 2 and the Output Submodule sheet) as visual reference — not reproduced here.

## Why this matters beyond this one circuit

This is a concrete instance of a broader pattern worth carrying forward: **simulate before trusting an assumption that affects signal integrity.** The "should be fine, it's just a voltage divider away from full range" assumption would have silently clipped the top 40% of the torque signal in the field. Independent simulation caught it before any board was fabricated.
