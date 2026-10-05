# Platypus One PCB workstream — design status

**Authoritative status for custom PCBs.** Last updated **2026-10-05**.
Tracking issue: [#33](https://github.com/Matthewjg95/PlatypusOne/issues/33).
Upstream gates (not duplicated here): [Rev A gates](../../docs/hardware/PERCEPTION_CARRIER_REV_A_GATES.md),
[pre-layout split](../../docs/hardware/PERCEPTION_CARRIER_REV_A_PRELAYOUT.md).

Nothing in this directory is bench evidence. Every value is tagged with where it
came from (datasheet, derived, estimate) or marked `BENCH_VERIFY` / `UNKNOWN`.

## What PCB are we designing?

**Perception Head PCB, Rev A** — a small rigid board at the top of the
handheld that carries the sensing stack as one calibrated unit:

| Function | Rev A implementation | Source sheet |
|---|---|---|
| ToF depth | Pololu #3419 VL53L8CX carrier, hosted on headers + 2× M2, own I2C bus, power-cyclable 3.43 V rail | `tof.kicad_sch`, `power.kicad_sch` |
| IMU | Bosch BMI270, I2C bus A, INT1 direct to host | `imu.kicad_sch` |
| Controlled white illumination | 2-channel linear constant-current sink, analog set-point (DAC/filtered PWM), current-limited switched 5 V, off-board light boards | `illumination.kicad_sch` |
| Identity / calibration | AT24CS32 EEPROM with factory 128-bit serial, write-protected by default | `id_calibration.kicad_sch` |
| Control I/O | TCA9534 expander for slow enables (ToF power, illumination enable, EEPROM WP, status LED) | `host_interface.kicad_sch` |
| Host link | one locking 14-pin JST GH cable (5 V, 3V3, 2× I2C, 3 interrupts/sync, illumination set) + Qwiic bench port | `host_interface.kicad_sch` |
| Camera | **mechanical only**: 4 standoff holes for the selected IMX219 module; CSI stays on the Media Carrier | `debug_test.kicad_sch` |
| Bring-up | 31 test points, 3 Kelvin current shunts, LED-current sense points, illumination kill jumper | `debug_test.kicad_sch` |

KiCad project: [`perception_head_rev_a/`](perception_head_rev_a/README.md) (KiCad 9.0).

**Host / Power / Controls Carrier (B)** is defined at architecture/interface
level only: [HOST_CARRIER_ARCHITECTURE.md](HOST_CARRIER_ARCHITECTURE.md).

## Why does it exist?

The camera↔ToF relative pose is a calibration quantity. If camera and ToF are
held by the enclosure, every display change, cover removal or chassis revision
can invalidate calibration. A rigid head PCB that also holds the camera module
makes the pose a property of one serviceable sub-assembly, independent of the
display decision (#41) that is still moving.

## What is Rev A supposed to prove?

1. ToF + IMU + illumination can run on the UNO Q MCU side **without disturbing**
   the proven RGB CSI path (Media Carrier CSI + DSI display).
2. The single-cable interface (power + 2 I2C buses + interrupts) is electrically
   clean at the real cable length.
3. Camera↔ToF extrinsic calibration survives remove/refit of the head
   (datum-hole scheme).
4. Controlled illumination at a measured current improves capture repeatability
   enough to earn its power/thermal cost — or is shown not to.
5. Per-unit identity + compact calibration record can be read by software and
   tied to captures.
6. Real power numbers for every head load, measured through on-board shunts.

## Explicitly out of scope (Rev A)

MIPI CSI routing on our board; replacing the Media Carrier; display interface;
battery/charging; bare VL53L8CX integration (Rev B candidate); radar, thermal,
RF experiments, robotics; cosmetics; production certification; algorithm tuning.

## Status board

| Item | State | Notes |
|---|---|---|
| Architecture split A/B | **FROZEN** | per pre-layout doc |
| Camera = replaceable module on Media Carrier CSI | **FROZEN for Rev A** | |
| ToF option: host Pololu #3419 (not bare IC) | **DECIDED for Rev A** | [ledger §VL53L8CX](RESEARCH_LEDGER.md#vl53l8cx-decision-host-the-pololu-3419-for-rev-a) |
| ToF on dedicated I2C bus + switchable rail | **DECIDED** | reset = power cycle (UM3109 §4.2) |
| IMU = BMI270 on bus A | **DECIDED** | BNO055 is NRND |
| Illumination topology (linear CC, analog set) | **PROVISIONAL – strong** | current, LED part, placement open |
| LED current / LED part / light-board geometry | **OPEN** | optical + thermal + host 5 V evidence |
| EEPROM identity scheme | **PROVISIONAL – strong** | record schema to define in software |
| Host connector family/pinout | **PROVISIONAL** | JST GH 14 for bench; final after Fusion routing |
| Host-side pin assignment | **PROVISIONAL (TBD_*)** | candidates in [ICD](ICD_PERCEPTION_HEAD.md) |
| ToF rail voltage 3.43 V | **PROVISIONAL – BENCH_VERIFY** | AVDD headroom vs I/O level |
| Board outline, hole positions, connector placement | **BLOCKED (MECHANICAL)** | [Fusion inputs](MECHANICAL_FUSION_INPUTS.md) |
| Camera choice (B0394/B0393/B0390) | **BLOCKED (BENCH)** | [intake table](CAMERA_EVIDENCE_INTAKE.md) |
| ToF usefulness / operating modes | **BLOCKED (BENCH, Platypus Lab #3)** | no physical ToF data yet |
| Power budget | **BLOCKED (BENCH)** | [power tree](POWER_TREE_REV_A.md) |
| Schematic ERC | **0 errors / 0 warnings** (2026-10-05, KiCad 9.0.9) | [report](perception_head_rev_a/outputs/erc_report.txt) |
| Layout / Gerbers / order | **NOT STARTED – gated** | |

## What can proceed now (no physical evidence needed)

- Firmware skeleton for the head on the STM32 side: TCA9534 safe-init order,
  BMI270 config upload, AT24CS32 record format + CRC, ToF power-cycle state machine.
- Calibration record schema (EEPROM + host files keyed by serial).
- Symbol/footprint verification of remaining stock parts against manufacturer drawings.
- Light-board concept (LED candidates, diffuser, baffle) as a separate tiny board.
- Host-carrier block diagram refinement once the display path narrows.

## Gate releases

**Schematic freeze** requires all of:
1. Host pin map frozen (UNO Q pins proven by a bench harness with this exact signal set).
2. Pololu 1×4 header order continuity-checked; AVDD ≥ 3.13 V under 8×8 ranging at the chosen rail voltage.
3. Bus B clean on a scope at the real cable length (with/without C18/C19).
4. Host 5 V headroom measured with UNO Q + Media Carrier + display + camera running → sets R15 (switch limit) and LED full-scale.
5. Illumination current + LED part chosen from optical/thermal tests.
6. Platypus Lab ToF evidence shows ToF earns its place (or ToF is dropped/deferred, which simplifies the board).
7. Every DECISION REQUIRED row in the [register](DECISION_REGISTER.md) closed or explicitly deferred.

**Layout start** additionally requires:
1. Fusion delivers head outline envelope, camera/ToF optical centres, datum-hole
   positions, connector exits and keep-out cones ([list](MECHANICAL_FUSION_INPUTS.md)).
2. Camera module selected and its board/holes/lens stack measured (#40 T5).
3. Footprints frozen (connector families final).

**Fabrication** additionally requires:
1. DRC clean with fab-house rules; 3D STEP fit-checked in Fusion.
2. Bring-up plan ([BRINGUP_AND_TEST.md](BRINGUP_AND_TEST.md)) reviewed; test fixture/harness ready.
3. BOM availability re-checked on order day (stock/lifecycle).
4. Independent review of the schematic (human or second agent) with this README's gates ticked.

## Document map

| Document | Purpose |
|---|---|
| [RESEARCH_LEDGER.md](RESEARCH_LEDGER.md) | Sources and exact facts used; decisions derived from them |
| [DECISION_REGISTER.md](DECISION_REGISTER.md) | Every unknown, classified, owned, with blocking impact |
| [ICD_PERCEPTION_HEAD.md](ICD_PERCEPTION_HEAD.md) | UNO Q / Media Carrier ↔ head interface, connector candidates |
| [POWER_TREE_REV_A.md](POWER_TREE_REV_A.md) | Rails, KNOWN / MEASURED / ESTIMATED / UNKNOWN loads, bench measurements needed |
| [BOM_PERCEPTION_HEAD_REV_A.md](BOM_PERCEPTION_HEAD_REV_A.md) | Candidate BOM with rationale and alternates |
| [MECHANICAL_FUSION_INPUTS.md](MECHANICAL_FUSION_INPUTS.md) | Optical/mechanical constraints and what Fusion must supply |
| [BRINGUP_AND_TEST.md](BRINGUP_AND_TEST.md) | Safe power-up, bring-up sequence, failure isolation tests |
| [CAMERA_EVIDENCE_INTAKE.md](CAMERA_EVIDENCE_INTAKE.md) | Drop-in table for the B0394/B0393/B0390 results |
| [HOST_CARRIER_ARCHITECTURE.md](HOST_CARRIER_ARCHITECTURE.md) | Board B architecture and interfaces (no layout) |

Directory layout is deliberately flat: add `manufacturing/` or `bringup/`
sub-directories only when they contain real files (fab outputs, logs).
