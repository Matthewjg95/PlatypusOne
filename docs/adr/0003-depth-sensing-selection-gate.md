# ADR-0003 — Gate depth-sensor selection on captured evidence

- **Status:** Proposed
- **Date:** 2026-09-20
- **Deciders:** Matthew (owner)

## Context

PlatypusOne needs range and depth evidence for guided multi-view capture,
foreground separation, scale context, and later Mesh2CAD reconstruction. The
current BOM leaves VL53L1X versus VL53L8CX open. Project Platypus is now
preparing a Tab5/M024 experiment using the Pololu VL53L8CX carrier, while ST's
new VL53L9CX offers a substantially denser 54×42 depth map and a longer
published range.

These devices answer different questions:

- VL53L8CX is a compact 8×8 I²C/SPI multizone sensor suitable for inexpensive
  embedded ranging and coarse depth evidence.
- VL53L9CX is a camera-class 2,268-zone dToF module whose full-rate output uses
  MIPI CSI-2 or I3C and whose integration requires multiple rails, a clock,
  thermal care, host support, and high-speed carrier routing.

The UNO Q has image processors and bottom high-speed connectors intended for
MIPI-CSI cameras, which makes an L9CX path plausible. That does not prove the
carrier exposes the required lane, that Linux accepts the sensor's
user-defined CSI payload, or that an RGB camera and depth sensor can operate
simultaneously.

Vendor specifications and chat research are not sufficient to freeze a product
BOM. Project Platypus must remain the owner of its experiment, and PlatypusOne
must explicitly decide whether those results transfer.

## Decision

PlatypusOne does **not** select VL53L8CX, VL53L9CX, or another ToF device at
this stage.

The product architecture shall preserve:

- a rigid, documented datum among RGB camera, depth sensor, IMU, and controlled
  illumination;
- a replaceable sensor pod or carrier region;
- access to a suitable low-speed sensor interface for the Rev A path;
- an unresolved high-speed camera-interface allocation until RGB plus depth
  coexistence is demonstrated;
- immutable, timestamped depth observations with validity/confidence metadata;
- calibration, a known-scale method, and user-locked dimensions independent of
  the chosen sensor.

VL53L8CX remains the practical Rev A candidate. VL53L9CX remains a candidate
for a no-compromise personal “Platinum” build. Neither status changes the BOM
or production default.

Selection occurs only after the evidence gates below pass and this ADR is
accepted or superseded by a selecting ADR.

## Evidence gates

### Gate A — Project Platypus VL53L8CX evidence

The Project Platypus test package must link raw evidence and report:

- 3.3 V rail behavior, average/peak current, and absence of 5 V on Tab5 I²C;
- sustained bus coexistence with touch and IMU;
- achieved resolution/rate and timeout/invalid-zone behavior;
- range, repeatability, fixed-pattern, ambient-light, dark-target, glossy, and
  glass behavior;
- mechanical camera/ToF alignment stability;
- whether depth improved mapping or camera context on paired scenes;
- failures and limitations, not only successful captures.

Passing Gate A permits a PlatypusOne-specific VL53L8CX evaluation. It does not
automatically select the part.

### Gate B — PlatypusOne capture relevance

Using the intended camera distance and representative engineering parts,
PlatypusOne must demonstrate that depth evidence improves at least one defined
workflow without degrading evidence integrity:

- foreground/background or occlusion separation;
- guided-view completeness;
- camera-view registration;
- scale/range sanity checking;
- uncertainty reporting;
- Mesh2CAD evidence export.

The test must retain raw RGB/depth data, calibration, transforms, sensor
settings, confidence/status data, and independently measured references.

### Gate C — VL53L9CX/UNO Q feasibility

Before the L9CX enters any BOM or enclosure freeze:

- evaluate an STEVAL-VL53L9 or equivalent supported carrier;
- prove the UNO Q carrier exposes a compatible one-lane MIPI CSI-2 or adequate
  I3C path;
- prove the Linux driver and data pipeline preserve depth, active/ambient IR,
  reflectance, and confidence as applicable;
- demonstrate simultaneous RGB and depth capture, or explicitly allocate a USB
  RGB camera while CSI is reserved for depth;
- measure rail demand, peak and sustained power, sensor temperature, and
  interference with nearby heat sources;
- verify timestamp synchronization and the rigid RGB-to-depth transform;
- quantify actual accuracy separately from temporal precision;
- confirm that the benefit justifies the carrier, software, thermal, and supply
  complexity.

## Consequences

- Current Project Platypus tests can proceed without silently redesigning
  PlatypusOne.
- The PlatypusOne BOM remains unchanged.
- Mechanical work preserves a rigid sensor datum and replaceable sensor region
  without freezing a specific aperture.
- High-speed interface allocation remains an explicit carrier-design question.
- Results are reusable because raw observations and limitations live in the
  experiment-owning repository.
- A failed L8CX test does not automatically reject L9CX; it identifies which
  failure belongs to the sensor, interface, mounting, or workflow.
- A successful L8CX test does not automatically justify L9CX.

## Authoritative inputs

- [Project Platypus VL53L8CX/M024 PR](https://github.com/Matthewjg95/project-platypus/pull/1)
- [Project Platypus ToF test plan](https://github.com/Matthewjg95/project-platypus/blob/docs/vl53l8cx-m024-depth-plan/docs/TOF_TEST_PLAN.md)
- [ST VL53L8CX product page](https://www.st.com/en/imaging-and-photonics-solutions/vl53l8cx.html)
- [ST VL53L9CX product page](https://www.st.com/en/imaging-and-photonics-solutions/vl53l9cx.html)
- [ST VL53L9CX datasheet](https://www.st.com/resource/en/datasheet/vl53l9cx.pdf)
- [ST STEVAL-VL53L9 evaluation board](https://www.st.com/en/evaluation-tools/steval-vl53l9.html)
- [Arduino UNO Q hardware overview](https://docs.arduino.cc/hardware/uno-q)

## Alternatives considered

| Option | Why not now |
|---|---|
| Select VL53L8CX immediately | Tab5 hardware is ordered but untested, and transfer to 10–30 cm part capture is unproven |
| Select VL53L9CX immediately | Camera-class interface, supply, clock, thermal, driver, and dual-camera allocation are unproven |
| Keep VL53L1X as the default | Low risk, but scalar range cannot provide the intended spatial depth evidence |
| Remove ToF from Rev A | Preserves schedule but gives up potentially valuable capture evidence before it is tested |
| Treat Project Platypus results as the decision | Cross-project evidence is useful, but environment and workflow differences require PlatypusOne review |
