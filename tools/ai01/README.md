# AI-01: planar image to EngineeringObservation

Small, deterministic Python prototype for [issue #19](https://github.com/Matthewjg95/PlatypusOne/issues/19).
No model provider, network inference, semantic guessing, or CAD output.

## Run from the repository root

Python 3.11+; a virtual environment is recommended:

```sh
python -m pip install -r tools/ai01/requirements.txt
python -m tools.ai01.capture --image tools/ai01/fixtures/plate.png --reference-mm 20 --output captures/plate-001
python -m tools.ai01.capture --camera 0 --reference-mm 20 --output captures/webcam-001
```

`--output` must be a **new** directory. Camera input captures one frame, releases
the device, and stores that decoded frame losslessly as PNG. For image input the
original file is copied byte-for-byte. The timestamp is the processing/capture
event time, not an invented original-photo time. Each event receives a UUID;
identical pixels/configuration produce identical geometry, while distinct events
have distinct metadata. SHA-256 and OpenCV version are recorded in `source`.

## Supported scene

- One solid, nearly axis-aligned dark square of known side length, and one
  separate dark planar subject on a uniform light background.
- Use the existing [calibration sheet](../../docs/hardware/calibration_sheet.pdf):
  print at actual size, verify with a ruler/calipers, remove the text strip.
- Reference and measured face must be coplanar; camera perpendicular to them.
  Avoid glare, shadows, blur and lens edges. Focus before invoking webcam capture.
- Filled-square detection is geometric, not decoded identity. A square part plus
  square reference is ambiguous and rejected. ArUco is not implemented.
- Components below 64 pixels are ignored as noise; enclosed voids below 16 pixels
  are ignored. Significant edge-clipped components, extra subjects, absent or
  multiple square candidates fail explicitly. Touching objects are unsupported
  and may not always be recognized as invalid; inspect the overlay.

These conditions are prerequisites, not claims the image alone can verify.
This is a controlled-scene prototype, not a general segmentation/metrology tool.

## Output and evidence

| File | Purpose |
| --- | --- |
| `EngineeringObservation.json` | Existing v0.1 scalar-claim contract; pending human review |
| `source.*` | Preserved input evidence |
| `capture-config.json` | Operator-supplied reference size, input hash and method/version |
| `geometry.json` | Calibration detection, contour, bounding corners, primitive candidates |
| `subject-mask.png` | Selected subject, with enclosed light regions retained |
| `debug.png` | Blue reference, green bounds, yellow line candidates, red void candidates |

The pure function `services.vision.python.ai01.analyze(image, reference_mm)` accepts
an 8-bit BGR array. Acquisition/file I/O is isolated in the CLI adapter.

Otsu thresholding and connected components select the scene. Reference scale is
`reference_mm / sqrt(reference_pixel_area)`, matching Scout's convention. Subject
bounds use an OpenCV minimum-area rectangle around **pixel cells**, not just pixel
centres. Pixel contours, line segments from a 1px polygon approximation, enclosed
light-region counts and circle-like voids are detector evidence. Circularity >=0.80
and bounding aspect >=0.90 select circular-void candidates; the equivalent diameter
is calculated from their pixel area. These are neither certified circles nor proof
of through-holes. Polygon edges are line candidates, not verified manufactured edges.

Physical dimensions are derived claims linked to pixel evidence and calibration
input. `*_pixel_sensitivity` is a conservative response to assumed ±1px boundary
movement at each endpoint, including reference-side variation. It is **not** a
validated accuracy bound or statistical confidence interval. Lens distortion,
perspective, print error and segmentation bias remain unquantified. No fabricated
confidence score is emitted. Thickness, material, hidden geometry, part class and
absolute accuracy remain unresolved with concrete next-capture recommendations.

Scene-analysis failures retain source/config plus `failure.json`, emit no success
JSON, and exit nonzero. Acquisition/decode/write failures also exit nonzero; a
partial directory may remain for inspection. Use another output directory to retry.

## Existing Scout path and PR #20

Inspection baseline: main `e931fd7`; PR #20 head `ff9a113` (housekeeping).
The PR documents already-existing C++ `ScoutAnalyzer` and `FastenerClassifier`
implementations; it does not introduce a replacement perception pipeline.

Reused boundaries: contract v0.1, scalar claim values, source string metadata,
artifact-relative paths, provenance, pending review, filled-square scene convention,
and area-based mm/px scale. The C++ parser/validator reads this Python output.

Intentional gaps:

- Python uses method `vision.ai01_planar.v1` and `p-*` IDs. It is not a numerical
  reimplementation of `vision.scout_analyzer.v2`: bounding rectangles differ from
  Scout's minimum-support-width / across-flats measurements. Python also rejects
  all multiple reference candidates and significant multiple subjects, whereas
  Scout ranks components.
- FastenerClassifier consumes typed `ScoutAnalysis` and depends on `sa-*` evidence.
  Feeding generic bounding widths into its nominal fastener matching would be
  misleading. No classifier binding is added; `inferred` remains empty. A future
  adapter must preserve Scout's measurement semantics and provenance or explicitly
  introduce a separately validated classifier input.
- There is no Python/C++ runtime binding, HAL integration or new observation schema.
  Arrays live in geometry artifacts because the C++ ClaimValue is scalar.
- Perspective rectification, lens calibration, ArUco, arbitrary backgrounds,
  multiple objects, depth/thickness and real-camera validation are deferred.

## Regression and acceptance

```sh
python -m unittest tools.ai01.test_ai01 -v
python -m mypy --strict --explicit-package-bases services/vision/python/ai01.py tools/ai01
cmake -S . -B build-ai01 -DPLATYPUS_BUILD_APPS=OFF -DPLATYPUS_BUILD_TESTS=ON
cmake --build build-ai01 --target ai01_validate_observation
build-ai01/tests/ai01_validate_observation captures/plate-001/EngineeringObservation.json
```

`fixtures/plate.png` is synthetic: 80px reference side, 160×80px plate, radius-12px
circular void, at 4px/mm. Regenerate with `python -m tools.ai01.fixture`. Tests verify
40×20mm bounds, ~6mm void, provenance/artifact existence, deterministic outputs,
input preservation, no overwrites, and explicit scene failures. CI also validates
the emitted JSON through the real C++ `fromJson` / `validate` implementation.

**Issue #19 remains open:** the synthetic fixture does not satisfy its real-capture
acceptance gate. Next: capture a flat plate/washer beside the verified 20mm square,
inspect debug/mask/calibration artifacts, compare dimensions to calipers, and record
human review and setup/error details. No camera or UNO Q was available to validate
this implementation during development.
