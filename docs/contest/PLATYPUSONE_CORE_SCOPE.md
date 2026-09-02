# PlatypusOne Core — Autodesk Contest Scope Envelope

Status: **scope baseline**  
Date: **2026-08-29**

BOM relationship: this is the authoritative **Contest Core Build** scope. It is distinct from the application/request BOM and the post-contest research inventory (scope layering is governed in the private planning overlay).

## Purpose

This document defines the bounded **PlatypusOne Core** submission for the Autodesk University 2027 Product challenge.

The 2025 Autodesk MultiMeasure is used only as a **manufacturing and product-complexity reference**. It is **not** a feature template. PlatypusOne must remain free to pursue a more ambitious engineering-workflow concept.

> **Rule:** inherit production discipline from prior Factory Experience products, not their feature set.

PlatypusOne's core thesis remains:

> **Make physical engineering work easier by extracting useful, structured information from imperfect real-world inputs.**

Anything required to prove that thesis reliably is **Core**. Anything that broadens the platform but is not needed to prove it is **Stretch / Full Product**.

---

## 1. What the MultiMeasure teaches us

Public Autodesk and RIT material describes the 2025 MultiMeasure as a compact handheld device combining a rotating arm, distance and color sensors, rotary encoder, and LCD display. Autodesk also documents that the final product required injection-molded parts, CNC and additive components, production circuit boards, and repeated mechanical refinement of the articulated arm.

Useful lessons:

1. **The conference product must feel like a product, not a dev-board demo.**
2. **A few coherent capabilities beat a pile of unrelated sensors.**
3. **Mechanical design can be part of the sensing strategy.**
4. **Assembly experience matters.** Parts, fasteners, bosses, cable routing, tolerances, and access should be intentional.
5. **Manufacturing variety is acceptable** when justified: molded/printed polymers, CNC parts, PCBs, and off-the-shelf electronics can coexist.
6. **Production refinement matters more than raw technical novelty.** A clever mechanism or workflow that works repeatedly is valuable.

What we explicitly do **not** infer:

- PlatypusOne does not need to copy angle/color/distance functions.
- PlatypusOne does not need a rotating arm.
- PlatypusOne does not need to match the MultiMeasure's shape, UI, sensor count, or architecture.
- The MultiMeasure is not a ceiling on creativity.
- Estimated cost, dimensions, and exact component choices are not treated as hard constraints unless Autodesk publishes them as contest requirements.

### Reference sources

- Autodesk Fusion Blog, *The AU Factory Experience Returns: Introducing the Autodesk MultiMeasure*, 2025-09-11: https://www.autodesk.com/products/fusion-360/blog/the-au-factory-experience-autodesk-multimeasure/
- RIT, *Student designs hit the factory floor at Autodesk University*, 2025-09-18: https://www.rit.edu/news/student-designs-hit-factory-floor-autodesk-university
- Autodesk Fusion Factory Experience: https://www.autodesk.com/campaigns/fusion-360/factory-experience

---

## 2. PlatypusOne Core product definition

**PlatypusOne Core is a handheld engineering observation instrument.**

It captures a physical object or condition, combines multiple observations when useful, separates measurement from inference, and produces a reusable engineering record rather than merely showing a sensor value.

The contest device should demonstrate the loop:

**physical target → intentional capture → sensor evidence → engineering interpretation → uncertainty/provenance → reviewed artifact → Autodesk Fusion handoff**

This is the defining differentiator from a traditional digital multi-tool.

### Core experience

A user should be able to:

1. point PlatypusOne at an engineering target;
2. deliberately trigger an observation;
3. capture visual + physical context;
4. obtain at least one defensible measurement or derived property;
5. receive a clearly labeled interpretation with confidence/uncertainty;
6. save the result as a structured Engineering Observation;
7. reopen/export that observation for later engineering work;
8. send reviewed, unit-aware measurements into a parameterized Autodesk Fusion design.

If the December prototype does this convincingly and completes the traceable Fusion handoff, the PlatypusOne contest concept is proven.

---

## 3. Core hardware envelope

These are **functional roles**, not locked part numbers. Current BOM candidates remain provisional until validated.

### Required for PlatypusOne Core

| Role | Why it is Core | Current path |
|---|---|---|
| **Arduino UNO Q** | Contest platform + local compute + deterministic I/O split | Required contest hardware |
| **Display** | Review observations/results at point of work | Selected 5 in DSI touch target + UNO Media Carrier |
| **Primary physical input** | Gloves/field-friendly deliberate capture and navigation | Trigger + encoder/buttons |
| **Camera** | Imperfect real-world visual input; foundation for Engineering Scout / future ShadowScan | UVC autofocus candidate |
| **Distance/depth reference** | Adds spatial depth context, target/background separation, and an independent camera-geometry cross-check | VL53L8CX-class 8×8 multizone ToF preferred; VL53L1X retained as fallback |
| **Orientation sensing** | Captures pose/gravity/reference-frame context | IMU |
| **Controlled illumination** | Improves repeatability and turns lighting into part of the measurement system | High-CRI LED(s) + driver |
| **Local storage** | Makes observations persistent and reproducible | microSD / local filesystem |
| **Portable power** | Required for handheld use | Li-ion/LiPo + regulated 5 V path |
| **Carrier/interconnect PCB** | Converts prototype wiring into a credible assembled product | Fusion Electronics / PCBWay |
| **Purpose-built enclosure** | Ergonomics, optical geometry, assembly, protection, contest requirement | Fusion + PCBWay |

### Not automatically Core

The existence of a BOM line does not make the feature mandatory for December.

- dedicated color sensor
- radar / Doppler sensing
- haptics
- audio interaction
- modular detachable scout camera
- additional RF hardware
- custom actuator/mechanical probes
- environmental sensors
- external accessory ecosystem

These can graduate into Core only if they materially improve the primary demonstration without threatening reliability, schedule, assembly, or clarity.

---

## 4. Core software envelope

The software must prove a **measurement-to-artifact pipeline**, not merely a launcher full of unfinished apps.

### Must exist

1. **Observation capture service**
   - camera acquisition
   - trigger event
   - timestamp/session identity
   - sensor context

2. **Engineering Observation contract**
   - observed
   - measured/derived
   - inferred
   - unresolved
   - provenance
   - confidence/uncertainty
   - links to source artifacts

3. **One excellent perception workflow**
   - Engineering Scout Q is the leading candidate because DigiKey development directly feeds this requirement.

4. **At least one independent physical measurement path**
   - e.g. ToF distance and/or IMU angle/orientation.
   - It should be recorded into the same Engineering Observation rather than living as a disconnected utility.

5. **Human-readable review UI**
   - source image
   - measured values
   - interpretations
   - unresolved fields
   - recommended next observation when appropriate

6. **Persistence and CAD-neutral export**
   - JSON as the canonical machine-readable Engineering Observation
   - referenced image(s), calibration evidence, units, coordinate frame, and uncertainty
   - adapter-ready measurement/geometry package without CAD-vendor assumptions

7. **Autodesk Fusion contest handoff**
   - require user review before measurements drive CAD
   - export named, unit-aware parameters from the canonical observation
   - import/update a prepared Fusion design through Parameter I/O or a bounded Fusion add-in
   - retain source-observation identity and confidence in the handoff
   - follow [CAD Handoff Architecture](../architecture/CAD_HANDOFF.md)

8. **Repeatable validation**
   - known targets
   - measured error
   - failure cases
   - uncertainty shown rather than hidden

---

## 5. Contest Core demonstration

The preferred Autodesk demo is an evolution of Engineering Scout, not a replacement for it.

### Minimum winning-shaped demo

A judge watches one uninterrupted sequence:

1. User picks up an unknown physical part or engineering feature.
2. PlatypusOne is aimed/positioned and the physical trigger is pressed.
3. Camera + relevant context sensors capture evidence.
4. The device calculates at least one physical measurement.
5. It derives or infers useful engineering information.
6. The UI explicitly distinguishes **OBSERVED / MEASURED / DERIVED / INFERRED / UNRESOLVED**.
7. If evidence is insufficient, PlatypusOne asks for a useful next observation rather than fabricating certainty.
8. The result is stored as a reusable, CAD-neutral Engineering Observation.
9. The user confirms which measurements may drive CAD.
10. A Fusion adapter imports the reviewed values into named parameters in a prepared Autodesk Fusion design.
11. The model updates, while the source image, units, uncertainty, and observation identity remain traceable.

### Stronger target

Show two related modes sharing the same architecture:

- **Engineering Scout:** identify/measure/document a physical component or feature.
- **Measure/Orient:** collect direct ToF + IMU evidence into the exact same observation record.

This makes the device visibly multi-capability while keeping the software story unified.

---

## 6. Explicit Core vs Stretch boundary

### CORE — required before adding stretch

- UNO Q integration
- handheld enclosure
- production-intent display/input
- camera
- controlled capture/illumination
- ToF or equivalent independent dimensional evidence
- IMU/reference-frame evidence
- Engineering Observation data contract
- Engineering Scout-derived primary workflow
- persistence/export
- repeatable validation
- Fusion mechanical design
- Fusion Electronics schematic/carrier PCB
- clean physical assembly
- contest-quality documentation/video

### STRETCH — add only after Core is reliable

- dedicated color measurement
- full ShadowScan geometry reconstruction
- automatic CAD generation
- multi-view reconstruction/fusion
- detachable/batteryless Engineering Scout module
- radar / machine-health sensing
- RF inspection / Antenna Lab integration
- voice assistant
- cloud synchronization
- generalized object recognition
- accessory/module bus ecosystem
- haptics beyond basic feedback
- advanced local AI models
- autonomous follow-up capture
- generalized CAD automation beyond the bounded Fusion parameter handoff
- swappable sensor cartridges

### FULL PRODUCT — roadmap, not contest obligation

The long-term PlatypusOne can become a general physical-engineering interface spanning measurement, inspection, geometry recovery, RF characterization, documentation, CAD handoff, maintenance, and modular sensing. Its canonical observation stays CAD-program agnostic; selectable adapters translate reviewed evidence into Fusion, FreeCAD, neutral formats, or future CAD targets.

The contest prototype is **evidence that this platform should exist**, not an obligation to ship the entire platform in December.

---

## 7. Design freedom guardrails

To prevent last year's product from hamstringing creativity:

1. **Do not add a feature because MultiMeasure had it.**
2. **Do not reject a feature because MultiMeasure did not have it.**
3. Compare against MultiMeasure only on:
   - product coherence
   - assembly clarity
   - manufacturability
   - perceived completeness
   - repeatability
   - portability
4. PlatypusOne should be allowed to be **more computationally ambitious** if the user experience remains simple.
5. Prefer software intelligence that reuses the same sensors over adding hardware merely to increase feature count.
6. Every new sensor must answer: **what engineering ambiguity does this remove?**
7. Every app must answer: **what reusable engineering artifact does this produce?**
8. Every mechanical feature must answer: **does it improve capture, ergonomics, calibration, protection, assembly, or manufacture?**

---

## 8. Production-complexity envelope

The MultiMeasure demonstrates that a Factory Experience product may reasonably include multiple manufacturing processes. PlatypusOne therefore does **not** need to constrain itself to a single-process plastic box.

For the contest submission, however, the preferred architecture is intentionally bounded:

- **1 primary compute board** — UNO Q
- **1 carrier/interconnect PCB**
- **1 display assembly**
- **~3 primary sensing domains** — vision, distance/depth, orientation
- **1 controlled illumination subsystem**
- **1 battery/power subsystem**
- **1 main enclosure assembly** with a small number of manufacturable structural/cosmetic pieces
- standard fasteners/connectors wherever possible

This is a **planning target, not a contest rule**. Deviations are allowed when they materially improve the product.

### Assembly target

Design toward a conference participant being able to understand the product architecture while assembling it without delicate rework.

Prefer:

- keyed connectors
- captured/standard fasteners
- accessible battery
- obvious PCB/display alignment
- minimal loose wiring
- mechanical datum features
- serviceable sensor windows
- no calibration step requiring specialized lab equipment

---

## 9. Current BOM implications

The existing BOM is already close to the Core envelope, but this scope document changes how we interpret it.

### Keep actively developing

- UNO Q
- DSI display
- encoder/buttons/trigger
- autofocus camera
- VL53L8CX-class 8×8 multizone ToF (VL53L1X fallback)
- IMU
- high-CRI illumination
- battery/power
- carrier PCB
- enclosure

### Treat as stretch unless justified later

- **TCS34725 color sensor** — useful, but presently too close to MultiMeasure lineage to deserve Core status on heritage alone. It must earn its place through a compelling Platypus workflow.
- **BGT24LTR11 radar** — remains V2/stretch.
- vibration motor — optional polish.
- speaker/audio — optional unless a concrete workflow proves it necessary.

Received research components (catalogued in the private planning overlay) remain outside Core unless explicitly promoted through §11.

This prevents a BOM from silently becoming a requirements document.

---

## 10. Relationship to DigiKey Dream Lab

`DIGIKEY_ENGINEERING_SCOUT_MVP.md` remains the September scope lock.

DigiKey proves the first perception vertical slice:

**physical part → controlled capture → measurement → inference → uncertainty → saved engineering record**

Autodesk then turns that software core into a coherent handheld product and adds physical context channels where they strengthen the workflow.

Therefore:

- DigiKey work is **not throwaway contest code**.
- Engineering Scout becomes the first flagship PlatypusOne Core workflow.
- Autodesk should reuse the observation contract, vision services, AI interface, filesystem/persistence layer, and UNO Q hardware interfaces.
- The post-DigiKey effort shifts from "invent another demo" to **productize, integrate, validate, and manufacture**.

---

## 11. Scope-change test

Before promoting any Stretch feature into Core, answer all five:

1. Does it reinforce the central PlatypusOne thesis?
2. Does it make the primary demo materially more compelling or reliable?
3. Can it be integrated without compromising the Core schedule?
4. Can it be explained to a judge in one sentence?
5. Does it produce or improve a reusable engineering observation/artifact?

If the answer is not **yes to at least four**, defer it.

Any scope change should be recorded in an ADR or this document rather than silently added to the BOM.

---

## 12. Definition of Done — PlatypusOne Core

PlatypusOne Core is contest-ready when:

- [ ] It is a self-contained handheld prototype built around UNO Q.
- [ ] Its industrial design is modeled in Fusion and physically manufactured.
- [ ] Its electronics/interconnect are documented and production-intent.
- [ ] One flagship observation workflow works repeatedly on real hardware.
- [ ] Vision is integrated as an engineering input, not a camera demo.
- [ ] At least one non-vision physical measurement is integrated into the observation pipeline.
- [ ] The UI distinguishes evidence, derivation, inference, and uncertainty.
- [ ] Observations persist as reusable, CAD-neutral artifacts.
- [ ] A reviewed observation updates named parameters in an Autodesk Fusion design with units and traceability preserved.
- [ ] Validation quantifies at least one meaningful performance metric.
- [ ] Failure behavior is demonstrated honestly.
- [ ] Assembly is clean enough to communicate a plausible ~750-unit product path.
- [ ] The build story shows Fusion mechanical + electronics work clearly.
- [ ] All contest documentation, BOM, schematics, source, photos, and video are complete.

At that point, **stop adding features and submit**.

---

## One-line scope lock

> **PlatypusOne Core is a manufacturable handheld that turns imperfect physical observations into measured, uncertainty-aware engineering records and drives a traceable Autodesk Fusion handoff; the underlying record remains CAD-neutral for the post-contest platform.**
