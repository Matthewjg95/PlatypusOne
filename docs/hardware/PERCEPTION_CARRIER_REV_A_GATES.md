# Rev A perception carrier — entry gates

> **Electrical design work (2026-10-05):** the Perception Head Rev A schematic,
> research ledger, decision register, ICD, power tree and bring-up plan live in
> [`hardware/pcb/`](../../hardware/pcb/README.md). This file remains the gate
> definition; `hardware/pcb/README.md` tracks which gates are released.

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


## Verified interface matrix — 2026-09-30

Sources for this section are manufacturer documentation; bench proof is still required before schematic freeze.

| Function | Rev A direction | Verified facts | Design consequence | Status |
|---|---|---|---|---|
| RGB camera | Media Carrier MIPI-CSI preferred; USB UVC remains known-good fallback | UNO Media Carrier provides two 22-pin, 4-lane MIPI-CSI connectors and explicitly lists IMX219 compatibility. UNO Q native camera path is four-lane MIPI-CSI-2 at 1.8 V I/O. | Prototype the supported IMX219-class CSI path before selecting a custom camera sensor. Keep UVC available until CSI capture is physically proven. | INTERFACE VERIFIED; SENSOR TBD |
| Display | Existing Media Carrier DSI path | Media Carrier provides 22-pin 4-lane MIPI-DSI. Physical 800×480 Waveshare panel path is documented separately in DSI_BRINGUP.md. | Carrier Rev A must coexist with Media Carrier rather than consume its display path. | PROVEN/STAGED |
| MCU sensor bus | MCU I2C4 / Qwiic candidate | UNO Q Qwiic is I2C4 (PD12/PD13) and 3.3 V only. Media Carrier preserves host signals and exposes MCU I2C4 on JMISC — but on a **different pin pair (PF14/PF15)** of the same peripheral, so it is not a second bus (2026-10-05 correction, see `hardware/pcb/RESEARCH_LEDGER.md`). | Use this bus for 3.3-V-compatible sensor interfaces; do not connect low-voltage bare-die I/O without translation. | VERIFIED |
| MCU control | MCU GPIO/PWM | Media Carrier exposes MCU GPIO at 3.3 V; UNO Q has additional STM32-controlled digital pins. | Prefer MCU control for illumination enable/PWM and deterministic sensor reset/interrupt handling. | VERIFIED; PIN ASSIGNMENT TBD |
| SoC GPIO | Linux-side control where required | Media Carrier exposes SoC GPIO at 1.8 V. | Never assume 3.3-V tolerance; use only where Linux ownership is necessary and level-match explicitly. | VERIFIED |
| ToF | VL53L8CX primary | 8×8 / 64-zone, up to 4 m, I2C up to 1 MHz or SPI up to 3 MHz, 3.3 V AVDD + 1.8 V core, IOVDD 1.2/1.8 V; GPIO1 interrupt and LPn control; ST publishes C API and Linux driver. | Bare IC requires low-voltage rail(s) and level compatibility. Rev A must either include regulation/translation or deliberately use a module that already solves them. Reserve INT + LPn. | PART DIRECTION STRONG; ELECTRICAL IMPLEMENTATION OPEN |
| IMU | BMI270 primary candidate | Current Bosch device; 1.7–3.6 V VDD, 1.2–3.6 V VDDIO, I2C/SPI, two interrupts, ~685 µA full ODR. | Native 3.3-V MCU-bus integration is plausible with VDD/VDDIO chosen accordingly. Fusion can run in software; no reason to anchor Rev A to BNO055. | PRIMARY CANDIDATE |
| IMU fallback | BNO055 removed as primary | Bosch marks BNO055 not recommended for new designs. | Keep only as dev inventory/fallback if already owned; do not design a new production carrier around it. | DEPRECATED FOR REV A |

### Immediate interface decisions

1. **Keep the UNO Media Carrier in Rev A architecture for now.** It already solves DSI and exposes two CSI camera connectors while preserving JMEDIA/JMISC signals. The custom perception carrier should initially complement it, not replace it.
2. **Prove an IMX219-class CSI camera on the Media Carrier before choosing a bare/custom RGB sensor.** A supported CSI module gives us a reference image pipeline and de-risks Linux/ISP support.
3. **Use the STM32 side for illumination/reset/interrupt ownership.** This keeps timing-sensitive physical control deterministic and avoids unnecessary dependence on 1.8-V SoC GPIO.
4. **Treat VL53L8CX voltage translation/regulation as a first-class schematic problem.** Its functionality fits the product, but the bare IC is not a 3.3-V Qwiic drop-in.
5. **Move BMI270 ahead of BNO055 for Rev A.** BNO055's integrated fusion is convenient, but its lifecycle status makes it the wrong anchor for a new custom PCB.

## Preliminary power ledger

This is intentionally incomplete. Unknowns are gates, not guessed numbers.

| Load | Rail / source | Known value | Budget status |
|---|---|---|---|
| UNO Q | 5 V system input | Arduino specifies 5 V / 3 A USB-C supply for board operation; whole-system measured load TBD | MEASURE |
| Media Carrier + DSI display | via UNO Q/carrier | system measured idle/active load TBD | MEASURE |
| RGB CSI camera | carrier camera rail(s) | depends on selected supported module | SELECT + MEASURE |
| VL53L8CX AVDD | 3.3 V | HP idle typ ~1 mA on AVDD; ranging value still to be entered from datasheet table before regulator sizing | INCOMPLETE |
| VL53L8CX CORE | 1.8 V | HP idle typ ~3 mA; ranging/peak still required | INCOMPLETE |
| VL53L8CX IOVDD | 1.2/1.8 V | low interface current; exact max from datasheet before freeze | INCOMPLETE |
| BMI270 | 1.7–3.6 V VDD; 1.2–3.6 V VDDIO | ~685 µA at full ODR | LOW RISK |
| White illumination | TBD switched rail | LED current/duty/thermal limit not selected | MAJOR OPEN ITEM |

**Power gate:** do not size the battery or carrier regulators from this table yet. First measure the proven UNO Q + Media Carrier + display baseline, select the CSI reference camera, and select the LED topology.
