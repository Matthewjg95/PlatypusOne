# sketch_intent — deterministic sketch intent resolver

Turns an imperfect measured 2D contour into an **editable sketch proposal**:
lines, arcs and circles plus the constraints (parallel, perpendicular,
coincident, collinear, tangent, equal radius, concentric) that the evidence
supports. Every inferred item has a residual, a decision and a reason. A person
reviews the proposal, and only accepted geometry is exported, through
Mesh2CAD's sketch-asset contract.

It does not "clean up" shapes. A 6° skewed quadrilateral stays skewed: the
perpendicular hypotheses are *rejected by evidence* and the reason is recorded.
A gap stays a gap unless a reviewer accepts a bridge, and a freeform edge stays
freeform. Original points are never modified.

Design, contract and decision rules: [docs/architecture/SKETCH_INTENT_RESOLVER.md](../../docs/architecture/SKETCH_INTENT_RESOLVER.md).
Benchmark (including failures): [docs/architecture/SKETCH_INTENT_BENCHMARK.md](../../docs/architecture/SKETCH_INTENT_BENCHMARK.md).

## Run

The only dependency is Python ≥ 3.11 with numpy. OpenCV is used only by the
benchmark's raster fixtures and external evaluation.

```sh
export PYTHONPATH=services/sketch_intent
python3 -m sketch_intent resolve contour.json -o proposal.json --svg proposal.svg
python3 -m sketch_intent show proposal.json              # summary, questions, unsupported items
python3 -m sketch_intent review proposal.json -o reviewed.json --by "Matthew" \
        --accept-proposed --reject c12 --accept c7        # questions need an explicit decision
python3 -m sketch_intent export reviewed.json --input contour.json -o part.sketch.json \
        --mesh2cad ../mesh2cad --dxf part.dxf --svg part.svg
python3 -m sketch_intent overlay reviewed.json --input contour.json -o accepted.svg
```

Demo, end to end on synthetic fixtures: `python3 services/sketch_intent/demo/run_demo.py`.

Tests: `python3 -m unittest discover -s services/sketch_intent/tests`. Set
`MESH2CAD_ROOT=<mesh2cad checkout with mra.sketch_assets>` to also validate
exports with Mesh2CAD's own model.

## Inputs

- `shadowscan-outline` (Tab5 applet / PlatypusOne `OutlineExport`, PR #29)
- ShadowScan Mobile `SketchJson` v1 (`raw.outer`, `raw.inner`)
- `platypus.contour_observation/1` (native; carries loop roles, open or closed
  state, observation id, calibration provenance and unresolved fields)

All points are in image pixels. When the input has no scale, the proposal stays
in pixels, says *insufficient calibration*, and export refuses.

## Layout

| module | role |
|---|---|
| `contract.py` | input adapters, `Params` (every threshold, recorded in each proposal) |
| `fitting.py` | TLS line and geometric circle fits |
| `segment.py` | outliers, gaps, breakpoints, line/arc/circle/freeform segmentation, competing corner interpretations |
| `constraints.py` | constraint hypotheses and their decisions (`decide`) |
| `solve.py` | applies chosen constraints jointly, builds junction vertices, discrepancy per entity |
| `resolve.py` | proposal assembly (`platypus.sketch_proposal/1`) |
| `review.py` | accept/reject rules, reviewed file with `parent_sha256` |
| `export.py` | accepted geometry → Mesh2CAD `SketchAsset` (schema 1.1) |
| `overlay.py` | SVG of the original evidence versus the proposed or accepted geometry |
| `bench/` | benchmark, `FREEZE.json` (held-out protocol) |
