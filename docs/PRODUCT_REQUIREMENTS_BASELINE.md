# Platypus One — Product and System Requirements Baseline

- **Baseline:** P1-PRB-0.1
- **Date:** 2026-08-25
- **Status:** Proposed
- **Owner:** Matthew
- **Change authority:** Owner approval through pull request or an accepted ADR

This document is the design anchor for Platypus One. It translates the product
idea, industrial-design direction, PlatypusOS architecture, hardware BOM, and
contest obligations into one traceable baseline. It does not freeze unresolved
parts or claim performance that has not been measured.

## 1. Requirement language and status

- **SHALL**: required for the stated baseline.
- **SHOULD**: expected unless testing or a documented trade study rejects it.
- **MAY**: permitted, not required.
- **Locked**: owner decision already made; change requires an ADR or baseline revision.
- **Target**: design objective requiring verification.
- **Open**: decision or numeric threshold is not yet resolved.
- **Deferred**: intentionally outside the contest Rev A baseline.

A requirement is not verified merely because code, CAD, or a BOM line exists.
Verification evidence must be linked or recorded against the requirement.

## 2. Product definition

### 2.1 Mission

**An engineer standing beside you.**

Platypus One is a one-handed, local-first physical-engineering observation
instrument. It converts imperfect real-world inputs—photos, geometry,
measurements, motion, and sensor observations—into structured engineering
evidence that can be reviewed, exported, and improved through iteration.

The primary interaction is:

> **Point → Understand → Visualize/Create**

The larger closed loop is:

> reality → sensor → local interpretation → evidence → engineering artifact →
> experiment or decision → improved knowledge → next observation

### 2.2 Product boundary

| ID | Requirement | Status |
|---|---|---|
| PROD-001 | Platypus One SHALL be a civilian engineering tool for inspection, measurement, reverse engineering, and documentation. | Locked |
| PROD-002 | The product SHALL NOT be designed or presented for weapons, military operations, harmful surveillance, or autonomous targeting. | Locked |
| PROD-003 | The product SHALL operate without a required cloud connection for its baseline workflows. | Locked |
| PROD-004 | The product SHALL remain useful when an optional sensor, camera, module, or network connection is absent. | Locked |
| PROD-005 | Every included feature SHALL save a recognizable step in a physical-engineering workflow; novelty alone is insufficient justification. | Locked |
| PROD-006 | Platypus One SHALL be treated as a product platform, not a single-purpose scanner or meter. | Locked |

### 2.3 Primary users

The baseline serves makers, product designers, manufacturing engineers,
technicians, inspectors, repairers, field engineers, and students who work
beside physical objects and later need trustworthy digital records.

The owner is the first ergonomic test user. His measured thumb-to-pinky hand
span is approximately **7.5 in**. That is a prototype fit input, not a
population-wide anthropometric requirement; broader fit testing remains open.

## 3. Baseline workflows

### 3.1 Contest Rev A workflows

| ID | Workflow | Required outcome | Status |
|---|---|---|---|
| WF-001 | Inspect | View a live or captured sensor observation and save it into a project record with source context. | Target |
| WF-002 | Measure | Capture a supported measurement, display its value and unit, and save it with time and source metadata. | Target |
| WF-003 | Document | Capture a photo and note without moving the evidence through a separate phone/notebook workflow. | Target |
| WF-004 | Visualize | Open and inspect a supported mesh or point-cloud artifact on the handheld. | Target |
| WF-005 | Capture geometry | Convert a controlled physical observation into a reviewable geometry artifact through the ShadowScan path. | Target |
| WF-006 | Export | Move a project artifact to a standard, user-owned file without a proprietary cloud dependency. | Target |

Rev A does not require every aspirational application to be equally mature. The
contest prototype SHALL demonstrate at least one complete closed loop from
physical observation to saved evidence to exported engineering artifact.
ShadowScan plus measurement is the preferred demonstration path in the current
roadmap.

### 3.2 Future physical-to-CAD handoff

Platypus One is the physical-world capture counterpart to Mesh2CAD and the
portable SketchAsset library. A future workflow MAY:

1. observe or scan a part;
2. identify a useful mechanical interface;
3. ask the user to confirm critical dimensions;
4. create a source-agnostic SketchAsset;
5. open or export that interface for Fusion, FreeCAD, or KiCad.

Automatic SketchAsset creation is **Deferred** from contest Rev A. The
requirements now are that Platypus One preserve enough provenance and units for
a future adapter, and that it not lock evidence to one CAD backend.

## 4. Functional requirements

### 4.1 Application platform

| ID | Requirement | Verification | Status |
|---|---|---|---|
| FUNC-001 | PlatypusOS SHALL present capabilities as applications sharing common platform services. | Architecture inspection + host test | Locked |
| FUNC-002 | The launcher SHALL identify applications whose required hardware is absent and prevent launch without preventing system boot. | Automated host test | Locked |
| FUNC-003 | Applications SHALL access hardware through HAL interfaces rather than concrete device drivers. | Dependency inspection | Locked |
| FUNC-004 | Application, service, and platform dependencies SHALL point downward only: apps → services → platform. | Build graph/review | Locked |
| FUNC-005 | The display geometry SHALL be discovered at runtime; apps SHALL NOT compile in one panel resolution. | Automated layout/build review | Locked |
| FUNC-006 | Long-running vision, geometry, or inference work SHALL NOT block the UI event loop. | Integration test | Target |
| FUNC-007 | User-facing operations SHALL expose cancel, failure, or unavailable states instead of silently hanging. | App acceptance tests | Target |

### 4.2 Projects and evidence

| ID | Requirement | Verification | Status |
|---|---|---|---|
| DATA-001 | A saved observation SHALL belong to an identifiable local project or explicit standalone record. | ProjectStore test | Target |
| DATA-002 | A record SHALL preserve timestamp, units where applicable, producing app, source capability, and device/software revision when available. | Schema test | Target |
| DATA-003 | Raw observations SHALL remain distinguishable from derived results. | Schema and export inspection | Target |
| DATA-004 | A derived claim SHOULD include the transformation, settings, and confidence or limitation information needed to interpret it. | Report inspection | Target |
| DATA-005 | Rejected, failed, or anomalous captures SHALL NOT be silently presented as valid evidence. | Fault-injection test | Target |
| DATA-006 | Export SHALL use documented, user-owned formats and SHALL NOT require a Platypus cloud account. | Offline export test | Locked |
| DATA-007 | Saving or exporting SHALL avoid silently overwriting an existing artifact. | Filesystem test | Target |
| DATA-008 | Local project data SHALL remain available after an ordinary restart or loss of network connectivity. | Restart/offline test | Target |

### 4.3 Measurement and capture

| ID | Requirement | Verification | Status |
|---|---|---|---|
| CAP-001 | A measurement view SHALL show value, unit, source, and current validity state together. | UI inspection | Target |
| CAP-002 | The user SHALL be able to deliberately trigger a capture with the hand holding the device. | Ergonomic test | Locked |
| CAP-003 | The camera and ranging axis SHOULD align with the natural pointing direction of the barrel. | CAD inspection + physical test | Locked |
| CAP-004 | Geometry or measurement output SHALL state when required calibration is missing or expired. | Fault-injection test | Target |
| CAP-005 | Accuracy and repeatability claims SHALL be based on recorded calibration and test data; no numeric accuracy is baselined yet. | Test report review | Open |
| CAP-006 | The user SHALL be able to review a captured artifact before it is accepted into the project record. | Workflow test | Target |
| CAP-007 | Controlled illumination SHALL be available to supported camera/ShadowScan workflows. | Hardware test | Target |

## 5. Human factors and industrial design

| ID | Requirement | Verification | Status |
|---|---|---|---|
| HF-001 | Rev A SHALL use a fat pistol-grip form intended for stable one-handed operation. | CAD + physical fit test | Locked |
| HF-002 | The primary grip orientation SHALL be left-hand-first so the dominant right hand remains available to probe, mark, or adjust the work. | Ergonomic test | Locked |
| HF-003 | The scan trigger SHALL fall under the gripping hand's index finger without requiring a grip shift. | Physical fit test | Target |
| HF-004 | The push rotary encoder SHALL be reachable by the left thumb without releasing the grip. | Physical fit test | Locked |
| HF-005 | The display SHALL be the largest practical screen supported by the front-face packaging and balance constraints. | CAD trade study | Locked direction |
| HF-006 | The charging connector SHALL exit at the grip base so a cable hangs away from the work area. | CAD inspection | Locked |
| HF-007 | Camera/ToF placement SHOULD minimize parallax and preserve intuitive aiming in the intended grip. | CAD + calibration test | Target |
| HF-008 | The device SHOULD support use on a bench through a kickstand and/or 1/4-20 tripod interface. | CAD + fit test | Target |
| HF-009 | Critical controls SHALL be distinguishable by position and shape without requiring the user to read the display. | Physical usability test | Target |
| HF-010 | Production-handedness strategy beyond the Rev A left-hand-first prototype remains open; software rotation/remapping MAY support broader use. | Owner decision | Open |

No final outer dimensions or mass are baselined. They SHALL be derived from
component packaging, grip fit, balance, manufacturing process, and physical
prototype testing rather than copied from an AI concept sheet.

## 6. Hardware and packaging requirements

### 6.1 Core architecture

| ID | Requirement | Verification | Status |
|---|---|---|---|
| HW-001 | Rev A SHALL use the Arduino UNO Q as the central compute, UI, storage, wireless, and sensor-fusion platform. | BOM + assembly inspection | Locked |
| HW-002 | Linux-side work SHALL own UI, vision, local inference, geometry, and project storage. | Architecture/bring-up test | Locked |
| HW-003 | STM32-side work SHALL own deterministic I/O and sensor sampling through the documented MCU bridge. | Bridge integration test | Locked |
| HW-004 | The enclosure SHALL reserve at least the UNO Q planning envelope of 85 × 54 × 20 mm until an exact board model supersedes it. | Fusion interference check | Target |
| HW-005 | Exact vendor STEP models SHALL replace planning envelopes before carrier PCB/enclosure freeze. | CAD review | Target |

### 6.2 Display

| ID | Requirement | Verification | Status |
|---|---|---|---|
| DISP-001 | The production path SHALL target a 4.3–5 in MIPI-DSI, 800×480-class capacitive panel driven from Linux. | ADR-0002 + bring-up | Locked |
| DISP-002 | The exact panel part, active area, connector, FPC routing, and Linux device-tree support SHALL be verified before the enclosure aperture is frozen. | Candidate report + bring-up | Open |
| DISP-003 | 800×480 SHALL be the UI reference geometry, not a compiled-in assumption. | Host geometry tests | Locked |
| DISP-004 | A linked external display MAY be used for prototyping; it SHALL remain behind IDisplay. | Integration inspection | Locked |
| DISP-005 | If no viable DSI panel is identified by 2026-09-30, the documented ESP32-S3 bridge fallback SHALL be reconsidered so the carrier schedule can hold. | Decision gate review | Locked gate |

### 6.3 Enclosure, RF, and thermal

| ID | Requirement | Verification | Status |
|---|---|---|---|
| MECH-001 | The prototype enclosure SHOULD use a hybrid CNC-aluminum barrel/screen surround and printed polymer grip. | CAD/BOM inspection | Target |
| MECH-002 | The grip SHALL remain polymer or otherwise RF-transparent around the radio antenna region. | CAD + RF test | Locked |
| MECH-003 | No metal enclosure feature SHALL cover the UNO Q radio antenna without a verified RF path or external-antenna design. | CAD review + connectivity test | Locked |
| MECH-004 | The forward barrel SHALL package the camera, illumination, and selected ranging sensor along the pointing axis. | CAD interference check | Locked |
| MECH-005 | Thermal coupling between hot compute components and any aluminum heat-spreading feature SHALL be intentional and electrically safe. | Thermal design review | Target |
| MECH-006 | Surface temperature, sustained compute load, and throttling limits SHALL be measured before claiming passive-heatsink performance. | Thermal test | Open |
| MECH-007 | Service access SHALL permit assembly and replacement of the battery, compute board, display, and carrier without destructive enclosure work. | Assembly trial | Target |

### 6.4 Planning envelopes

These are packaging inputs, not final part specifications:

| Component | Current envelope |
|---|---:|
| Arduino UNO Q | 85 × 54 × 20 mm |
| Production display class | approximately 120 × 75 × 5 mm at 4.3 in; exact part TBD |
| Camera candidate | 25 × 25 × 10 mm |
| Rotary encoder | 24 × 24 × 30 mm |
| Flat battery candidate | 100 × 60 × 8 mm |
| ToF/IMU/color sensors | exact selected breakouts TBD |

### 6.5 Power

| ID | Requirement | Verification | Status |
|---|---|---|---|
| PWR-001 | Rev A SHALL use a protected rechargeable cell and a power path capable of supporting the measured UNO Q, display, sensor, and peak-compute load. | Electrical test | Target |
| PWR-002 | USB-C charging/power SHALL be accessible at the grip base. | CAD + charge test | Locked |
| PWR-003 | The system SHALL preserve or safely close project writes before an intentional shutdown when the platform provides adequate warning. | Power-loss test | Target |
| PWR-004 | The published runtime SHALL be based on a repeatable workload test; approximately two hours is a planning target, not a verified requirement. | Runtime test | Open |
| PWR-005 | Battery protection, conductor sizing, charging temperature range, and enclosure clearances SHALL be reviewed before untethered use. | Electrical safety review | Target |

## 7. Software and interface requirements

| ID | Requirement | Verification | Status |
|---|---|---|---|
| SW-001 | PlatypusOS SHALL retain host-first operation so platform and app work can continue without target hardware. | Host build/test | Locked |
| SW-002 | Platform code SHALL use C++20; Python MAY be used for AI/vision prototyping behind a C++ service boundary. | Build/review | Locked |
| SW-003 | HAL boundaries SHALL remain exception-free and return explicit Result/Status values. | API inspection | Locked |
| SW-004 | Camera and sensor callbacks SHALL hand data to the UI through a thread-safe event mechanism. | Concurrency test | Target |
| SW-005 | Applications SHALL use runtime display information and SHOULD remain usable at the 800×480 reference geometry. | Host UI tests | Target |
| SW-006 | Hardware-specific types SHALL appear only in board implementations and the composition root. | Dependency inspection | Locked |
| SW-007 | No baseline workflow SHALL require a cloud API, account, or subscription. | Offline system test | Locked |
| SW-008 | Dynamic third-party app loading, signed manifests, OTA, and full power management are Deferred to M4 unless pulled forward by a documented blocker. | Roadmap review | Deferred |

## 8. Engineering Scout Module family

Platypus One is the brain; interchangeable Engineering Scout Modules are a
future field-extension family. Candidate concepts include Scout-Cam, Scope, RF,
Thermal, Light, Measure, and Tag.

| ID | Requirement | Verification | Status |
|---|---|---|---|
| MOD-001 | Rev A SHALL reserve an expansion concept and accessible packaging region but is not required to ship a detachable Scout Module. | CAD review | Deferred implementation |
| MOD-002 | A future dock SHOULD combine mechanical retention, power, data, module identification, and energy management. | Interface trade study | Deferred |
| MOD-003 | Module absence or failure SHALL NOT prevent the base device from booting. | Fault test | Locked architecture |
| MOD-004 | Module capabilities SHOULD be discoverable through the same capability model used for onboard sensors. | Architecture prototype | Deferred |
| MOD-005 | Supercapacitor-only or dock-charged single-capture modules remain research concepts and SHALL NOT be treated as a Rev A commitment. | Baseline review | Deferred |
| MOD-006 | Modules MAY use energy from the base device, a dedicated battery, a small local store, or harvesting only after the energy budget and safety behavior are documented. | Module-specific verification | Deferred |

## 9. Quality, reliability, safety, and privacy

| ID | Requirement | Verification | Status |
|---|---|---|---|
| QUAL-001 | The host build and automated tests SHALL pass before a release candidate is labeled verified. | CI/build record | Target |
| QUAL-002 | Target-hardware features SHALL remain marked unverified until exercised on the actual board or an accepted representative fixture. | Status review | Locked |
| QUAL-003 | A capture failure SHALL leave the previous accepted project artifact recoverable. | Fault-injection test | Target |
| QUAL-004 | Calibration data and software/firmware revision SHOULD be included with measurements that depend on them. | Export review | Target |
| SAFE-001 | The product SHALL provide no autonomous actuation of machinery in Rev A. | Architecture inspection | Locked |
| SAFE-002 | Sensor or inference output SHALL be framed as engineering assistance, not an unqualified safety certification. | UI/report review | Locked |
| SAFE-003 | The enclosure SHALL prevent ordinary user contact with energized conductors and retain the battery against movement or puncture. | Physical safety inspection | Target |
| PRIV-001 | Camera, project, and measurement data SHALL remain local by default. | Offline/privacy test | Locked |
| PRIV-002 | Any future network transfer SHALL be a deliberate user action with an identifiable destination. | Workflow review | Target |
| PRIV-003 | The product SHALL NOT conceal recording state during camera or audio capture. | UI/indicator test | Target |

Environmental ingress, drop height, operating temperature, chemical
resistance, ESD immunity, and regulatory targets are **Open**. Rev A SHALL NOT
claim a rating that has not been tested.

## 10. Contest and milestone constraints

| ID | Requirement | Verification | Status |
|---|---|---|---|
| CON-001 | The hardware application package SHALL include an original Fusion design/dataset (.f3d), preliminary BOM naming UNO Q and Fusion, original cover work, and English project copy. | Application checklist | Locked |
| CON-002 | The owner, not an agent, SHALL submit the Hackster application and final entry. | Process check | Locked |
| CON-003 | The hardware application target date is 2026-09-04 ahead of the reported 2026-09-07 deadline; the owner SHALL verify the live contest page. | Owner confirmation | Open external verification |
| CON-004 | The final contest submission target is 2026-12-16 through 2026-12-18 ahead of the reported 2026-12-20 deadline. | Submission record | Target |
| CON-005 | The contest enclosure and carrier design SHALL use Autodesk Fusion/Fusion Electronics. | Native files + exports | Locked |
| CON-006 | AI-generated concept sheets SHALL remain references only; contest-facing art and CAD SHALL be original work. | Submission review | Locked |

## 11. Rev A acceptance baseline

Platypus One Rev A is acceptable for the contest prototype when all of the
following are true:

1. A reviewable Fusion assembly packages the UNO Q envelope, selected display
   envelope, camera, ranging sensor, encoder, trigger, battery, USB-C exit, RF
   window, and major service clearances without unresolved hard interference.
2. The physical prototype can be held in the left hand, aimed, triggered, and
   navigated with the encoder without a grip change during a representative
   task.
3. PlatypusOS boots with absent optional hardware, reports capability
   availability, and runs at the actual display geometry.
4. At least one complete physical-observation workflow produces a saved local
   project record and a standard exported artifact.
5. The measurement/capture result distinguishes raw evidence, derived output,
   units, validity, and known limitations.
6. Power, thermal, display, camera, and selected sensor bring-up evidence is
   recorded against the hardware checklists.
7. A repeatable demonstration and recovery path exists; a failed capture does
   not destroy the previous accepted result.
8. Contest deliverables include native Fusion files, BOM, schematics where
   applicable, code, original photos/video, and a candid account of measured
   performance and limitations.

## 12. Open decisions and decision gates

| Decision | Needed by | Blocks |
|---|---|---|
| Exact DSI panel, FPC, touch controller, and Linux support | 2026-09-30 gate | aperture, display driver, enclosure depth |
| ToF: VL53L1X vs VL53L8CX | carrier freeze | placement, first ranging driver |
| IMU: BNO055 vs BMI270 | carrier freeze | placement, first IMU driver |
| Camera: UVC OV5640-class vs higher-resolution module | camera-path confirmation | barrel window, driver |
| Trigger switch technology | carrier/enclosure freeze | control placement |
| Expansion connector and reserved module envelope | carrier/enclosure freeze or explicit descope | module dock |
| Exact battery and validated power budget | enclosure freeze | grip volume, runtime |
| Production handedness strategy | after Rev A ergonomic test | future enclosure/control layout |
| Numeric measurement accuracy/precision thresholds | after sensor selection and calibration plan | published claims |
| Environmental and drop targets | post-contest product planning | material, sealing, compliance |

## 13. Traceability sources

This baseline consolidates, but does not replace:

- [PlatypusOS architecture](ARCHITECTURE.md)
- [Project status](../STATUS.md)
- [Roadmap](ROADMAP.md)
- [Industrial design notes](hardware/INDUSTRIAL_DESIGN.md)
- [Hardware BOM](hardware/BOM.md)
- [Hardware acquisition roadmap](hardware/ACQUISITION_ROADMAP.md)
- [Hardware application checklist](contest/HARDWARE_APPLICATION_CHECKLIST.md)
- [ADR-0001: dynamic linked prototype display](adr/0001-dynamic-linked-prototype-display.md)
- [ADR-0002: MIPI-DSI production display](adr/0002-dsi-production-display.md)
- [MCU bridge protocol](protocols/mcu-bridge.md)

When a source conflicts with this baseline, do not silently select one. Record
the conflict, determine which decision is newer, and update the baseline or
write an ADR.

## 14. Change control

- Requirement IDs are stable once this baseline is accepted.
- Wording may be clarified without changing intent.
- Adding, deleting, or materially weakening a SHALL requires owner approval.
- A cross-layer or hardware-direction change requires an ADR.
- A requirement may move to Deferred only with a documented consequence and
  recovery path.
- Verified status requires evidence; implementation alone is not evidence.
