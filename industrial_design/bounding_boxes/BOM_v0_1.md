# Platypus One BOM v0.1 — packaging envelopes

> Canonical parts BOM (with alternates + acquisition tracking):
> [docs/hardware/BOM.md](../../docs/hardware/BOM.md). This file tracks
> **packaging envelopes only** until vendor STEP models replace them.

| Component | Candidate | Envelope (mm) |
| --- | --- | --- |
| Compute | Arduino UNO Q | 68.58 x 53.34 board outline (official mech. drawing); 3D clearance TBD from STEP |
| Display | 5-7 in LCD | 165 x 100 x 8 |
| Camera | 13 MP autofocus | 25 x 25 x 10 |
| ToF | VL53L8CX | 6 x 6 x 3 |
| Radar | Grove BGT24LTR11 | 40 x 20 x 12 |
| IMU | BMI270 | 10 x 10 x 3 |
| Rotary Encoder | EC11-style | 24 x 24 x 30 |
| Battery | Flat Li-ion | 100 x 60 x 8 |

Next action: replace every envelope with an exact vendor STEP model.

> **UNO Q note:** production CAD must use Arduino's official STEP model /
> mechanical drawing (board outline 68.58 × 53.34 mm), not an approximate
> envelope. Board outline (2D) and required 3D clearance are separate
> concepts — never derive a component height from the outline.
