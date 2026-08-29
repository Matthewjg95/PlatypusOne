# Mesh2CAD Handoff and PlatypusOne BOM Traceability

Status: proposed design input  
Date: 2026-08-28  
Related: [Product requirements](PRODUCT_REQUIREMENTS_BASELINE.md), [hardware BOM](hardware/BOM.md), [Mesh2CAD capture contract](https://github.com/Matthewjg95/mesh2cad/blob/codex/geometry-research-2026-08-28/docs/PLATYPUSONE_CAPTURE_CONTRACT.md)

## Decision

PlatypusOne is the physical-evidence capture endpoint for the broader Mesh2CAD workflow. Rev A should collect a calibrated, reviewable evidence bundle and export it to a workstation. Full mesh-to-CAD reconstruction, bounded optimization, CAD-kernel validation, and discrepancy analysis remain off-device.

This decision directly influences the BOM:

- spend hardware budget on repeatable capture, scale, calibration and rigid sensor alignment;
- do not add a dedicated GPU/NPU solely for CAD reconstruction;
- select sensors by the evidence they preserve, not by whether each can claim an independent app;
- preserve the existing optional-hardware architecture: a missing ToF or IMU must reduce evidence quality transparently, not prevent boot.

## Research-derived BOM decision matrix

| Capability | Required for Rev A | Selection rule | Current direction |
|---|---|---|---|
| Camera | Yes | V4L2/Linux path, repeatable 10–30 cm capture, calibration support, exposure/focus lock or state reporting; megapixels are secondary | Keep OV5640-class UVC as schedule baseline; select 13 MP only if the same controls and UNO Q path are verified |
| Range | Yes for measurement; recommended for geometry evidence | Valid timestamped range; multi-zone data adds a coarse depth sanity map but is not treated as a mesh scanner | Prefer VL53L8CX for ShadowScan value if driver/bring-up fits schedule; VL53L1X remains fallback |
| Orientation | Yes for guided multi-view capture | Orientation attached to each deliberate capture without requiring new contest-critical sensor fusion | Keep BNO055 as schedule-first candidate |
| Illumination | Yes | Two fixed, independently controllable sources; state recorded with every capture | Clarify BOM item 12 and barrel mounting |
| Calibration/scale target | Yes | Printed checkerboard/fiducial plus a known dimension in the capture plane | Add as a physical accessory |
| Rigid sensor datum | Yes | Camera, ToF and lights share a mechanically stable mounting reference whose geometry is documented | Add to enclosure/carrier acceptance criteria |
| Stable support | Recommended | 1/4-20 insert and/or kickstand for repeatable capture | Add explicit insert hardware |
| Reconstruction accelerator | No | Rev A exports evidence to Mesh2CAD workstation; on-device work is capture, preview and lightweight preprocessing | No BOM addition |

## Evidence-quality gates before component freeze

### Camera

A camera candidate passes only if:

- it enumerates through the selected UNO Q path;
- still frames and stream frames can be saved with resolution and timestamp;
- focus is repeatable at 10–30 cm;
- exposure and focus can be locked, or their state can be recorded reproducibly;
- a checkerboard calibration can estimate intrinsics and distortion;
- three sequential captures do not silently change framing or orientation metadata.

### ToF and IMU

- Camera, ToF and IMU observations must use timestamps that can be associated with one deliberate trigger.
- Sensor validity must be stored alongside values.
- The relative transforms among camera, ToF and device frames must be recorded after assembly.
- Multizone ToF is supplementary evidence; it must not be represented as dimensional ground truth beyond measured performance.

### Mechanical and illumination

- Camera, ToF and illumination mounts must not shift during ordinary handling or enclosure service.
- Their mounting relationship must be dimensioned in Fusion before enclosure freeze.
- Illumination state and geometry must be repeatable.
- The forward window must avoid camera vignetting, ToF obstruction and uncontrolled reflections.
- The scale target and tripod insert must appear in the reconciled BOM even if inexpensive.

## Required exported evidence

The device should preserve:

- immutable raw images;
- view label and orientation;
- ToF range/depth samples;
- camera calibration reference;
- camera/ToF/IMU transforms;
- illumination state;
- user-confirmed dimensions and units;
- derived silhouettes/edge maps with settings and confidence;
- explicit capture limitations.

The versioned interface is documented in Mesh2CAD's `PLATYPUSONE_CAPTURE_CONTRACT.md`.

## First cross-repository proof

Capture a simple bracket from three guided views with a scale target, lock one dimension, export the evidence bundle, and reconstruct it in Mesh2CAD. The proof passes when Mesh2CAD returns an editable model, a feature-linked missing/excess-material report, and explicit uncertainty for hidden geometry without modifying the raw capture.
