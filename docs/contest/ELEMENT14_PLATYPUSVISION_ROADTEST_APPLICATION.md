# element14 Open Call RoadTest — PlatypusVision Application

Status: **Submitted — selection pending**  
Date submitted: **2026-08-30**

## Purpose

This document records the submitted element14 Open Call RoadTest application and preserves the technical opportunity without assuming selection.

No PlatypusOne Core BOM decision is changed by this application. The VL53L9CX remains a **candidate depth-sensing component** until either RoadTest hardware is received and evaluated or equivalent independent validation is completed.

## Submitted product

- **RoadTest name:** PlatypusVision: VL53L9CX 3D ToF for Engineering Observation
- **Distributor:** Newark / element14
- **Newark order code:** `12AN8629`
- **Manufacturer part number:** `X-NUCLEO-53L9A1`
- **Target sensor:** STMicroelectronics `VL53L9CX`

## Why this matters to PlatypusOne

PlatypusOne Core already requires an independent physical depth/reference channel alongside the camera and IMU. The RoadTest is intended to answer whether a denser 3D ToF sensor materially improves the Engineering Observation workflow enough to justify its cost, package size, interface complexity, and integration effort compared with lower-cost ToF alternatives.

The evaluation should inform the **component roadmap**, not force the X-NUCLEO development board into the final handheld enclosure.

## Proposed experiment — PlatypusVision

Use the VL53L9CX as the depth-sensing core of a bounded prototype called **PlatypusVision**.

Primary questions:

1. How accurate and repeatable is the sensor across useful handheld working distances?
2. How does performance change with engineering-relevant materials and surface finishes?
3. How does target angle, curvature, feature size, and ambient illumination affect usable depth data?
4. Does dense depth information materially improve useful interpretation compared with a simple single-point or low-resolution ToF sensor?
5. Can RGB + depth observations improve geometry, dimension, and feature reasoning for an engineering-observation workflow?
6. Do the benefits justify integration of the VL53L9CX-class sensor into the final PlatypusOne carrier PCB and enclosure?

## Planned target set

Include representative real-world engineering objects and surfaces such as:

- matte and glossy plastics
- dark / black surfaces
- aluminum and steel parts
- PCBs and electronic assemblies
- reflective fasteners
- curved objects
- angled faces
- small features and edges
- mixed-material assemblies

## Planned measurements

- setup and software experience
- raw depth visualization
- usable range
- repeatability
- frame rate / responsiveness
- field of view
- surface-finish sensitivity
- angular sensitivity
- small-target behavior
- indoor versus bright-light behavior
- RGB/depth alignment experiments
- practical engineering-object case studies
- failure cases and uncertainty
- comparison with simpler / lower-cost ToF approaches where practical

## Camera relationship

The RoadTest plan assumes pairing the depth sensor with an **autofocus RGB camera**. This reinforces the current direction that autofocus camera capability belongs in PlatypusOne Core.

The final camera part number is not locked by this RoadTest. Desired Core characteristics remain approximately:

- USB UVC / Linux-compatible path preferred for UNO Q integration
- autofocus
- at least ~5 MP class resolution
- compact module/PCB
- mechanically rigid relationship to ToF sensor and controlled illumination
- known / calibratable field of view

## Selection gate

### If selected

- Treat the RoadTest as a formal component-downselect experiment.
- Build the smallest practical PlatypusVision demonstrator.
- Collect reusable validation datasets and failure cases.
- Feed results into the PlatypusOne depth-sensor BOM decision and uncertainty model.
- Publish the element14 RoadTest on schedule.

### If not selected

- Keep VL53L9CX on the component roadmap as a higher-performance candidate.
- Do not purchase or integrate it automatically.
- Continue with the current lower-cost ToF fallback until project needs justify a dedicated comparison.

## Scope guardrail

This application does **not** expand PlatypusOne Core. It evaluates a higher-performance implementation of an already-required function: physical depth / dimensional evidence.

The RoadTest hardware is an evaluation platform only. The final product should use a component-level implementation on a purpose-built carrier PCB if the sensor earns its place.
