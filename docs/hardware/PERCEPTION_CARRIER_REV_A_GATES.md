# Rev A perception carrier — entry gates

Status: planning gate, 2026-09-30. This document prevents schematic/layout work from outrunning verified interfaces.

The custom carrier is the primary Platypus One path. Integrated commercial RGB-D modules are reference/fallback architectures, not the default.

## Mission

Create one mechanically rigid perception head that can eventually combine:
- RGB image capture
- multizone ToF depth
- IMU
- controlled white illumination
- calibration/provenance data

The carrier exists to make the sensing stack repeatable and serviceable. It must not become a second general-purpose compute board.

## Gate 0 — proven workflow

Before carrier layout, preserve the current working reference workflow:
`touch → RGB capture → Engineering Observation → display → saved evidence`.

A carrier revision may not remove that fallback path until its replacement has equivalent physical evidence.

## Gate 1 — UNO Q interfaces

Must be VERIFIED from authoritative documentation or bench evidence:
- [ ] exact camera interface chosen: USB/UVC or supported MIPI CSI path
- [ ] connector/pin mapping and voltage domains
- [ ] available I2C buses and logic levels for ToF/IMU/control
- [ ] GPIO/PWM path for illumination enable/dimming
- [ ] whether any interface conflicts with Media Carrier/display use
- [ ] reset/interrupt requirements for each sensor
- [ ] Linux driver availability and device-tree/configuration requirements

**No schematic freeze while any chosen device depends on an assumed UNO Q interface.**

## Gate 2 — power

Create a measured/traceable budget with:
- UNO Q + Media Carrier/display baseline
- RGB camera typical/peak
- ToF typical/peak
- IMU
- LEDs at maximum allowed duty
- carrier losses/regulators
- startup/inrush margin

Define rails, source, current capability, sequencing, decoupling and protection. Unknown values stay marked unknown; they are not replaced with guesses.

## Gate 3 — optical/mechanical datum

Fusion must receive a single sensor-head envelope containing:
- camera optical center and axis
- ToF optical center/axis and FOV
- LED locations and keep-outs
- window/lens clearances
- rigid mounting datums
- cable bend/connector service space

Camera↔ToF relative pose must be mechanically repeatable enough that calibration survives normal assembly/service.

## Gate 4 — calibration and synchronization

Define before layout:
- per-unit calibration data that must be stored
- schema/version identifier
- where calibration is stored and how software associates it with hardware
- camera↔ToF extrinsic calibration strategy
- timestamp source for RGB/ToF observations
- acceptable synchronization error for Rev A use cases

Rev A does not need laboratory metrology; it does need provenance and repeatability.

## Gate 5 — software support

For each primary part record:
- kernel/driver/API
- known working example
- expected device enumeration
- smallest independent bring-up test
- fallback part/interface
- risk: low / medium / high with reason

Prefer a less impressive sensor with a known Linux path over a superior part that makes Rev A depend on writing a camera stack.

## Gate 6 — observability/recovery

The carrier must expose enough access to debug itself:
- test points for every rail
- accessible I2C/GPIO where practical
- independent sensor reset/disable where useful
- current measurement strategy
- connector orientation/pin-1 markings
- safe fallback that lets RGB capture continue if ToF/IMU fails

## PCB entry criteria

Schematic work may start when Gates 1–5 have no architecture-level unknowns.

Layout/fab may start only when:
1. interface/pin table is frozen,
2. power budget and rails are frozen,
3. primary parts and footprints are frozen,
4. Fusion provides the sensor-head datum/envelope,
5. software path for every required device is credible,
6. calibration storage/synchronization approach is defined,
7. a bring-up and recovery plan exists.

## Explicitly out of Rev A carrier scope

- radar
- thermal imaging
- RF experimentation
- energy harvesting
- generalized expansion backplane
- production certification
- autonomous robotics

Those can use future modules. Rev A proves the Platypus One perception head.
