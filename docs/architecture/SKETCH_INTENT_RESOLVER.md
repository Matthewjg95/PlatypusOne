# Sketch Intent Resolver

Status: **first implementation, synthetic evidence only.** Code:
[`services/sketch_intent`](../../services/sketch_intent/README.md). Benchmark:
[SKETCH_INTENT_BENCHMARK.md](SKETCH_INTENT_BENCHMARK.md). Issue #37.

The resolver turns an imperfect measured 2D contour into an editable sketch
proposal. The proposal keeps the original observation unchanged, labels every
inferred item, shows its discrepancy from the evidence, and needs a person's
review before anything is exported. It must not quietly perfect shapes.

```
contour observation ──► primitive hypotheses ──► constraint hypotheses ──► proposal
 (immutable points,       line / arc / circle /     parallel, perpendicular,   geometry + residuals +
  scale + provenance)     freeform, outliers, gaps  coincident, collinear,     questions + provenance
                                                    tangent, equal radius,          │
                                                    concentric                       ▼
                    Mesh2CAD SketchAsset ◄── export ◄── accepted geometry ◄── review (accept / reject)
                    (DXF / SVG via mra.sketch_assets)
```

## 1. Canonical home

| Option | Verdict |
|---|---|
| **PlatypusOne `services/sketch_intent` (Python + numpy)** | **Chosen.** Issue #37's architecture rule: "a reusable geometry/intent service, not in the fastener classifier or Scout UI." It runs unchanged on the UNO Q's Debian side and on a desktop. Mesh2CAD (Python) can import it or vendor it. |
| Mesh2CAD `mra/intent` | Close in spirit: `recover.py` is the policy this follows. But Mesh2CAD is a mesh→CAD tool with its own `IntentDocument` (PR #6, experimental). Putting a 2D contour resolver there would make PlatypusOne depend on a repo it doesn't own for its core pipeline. |
| ShadowScan Mobile (Kotlin) | Rejected. The ShadowScan handoff says "do not make the Android UI the owner of CAD semantics." ShadowScan shares the **contract and fixtures**; a Kotlin port, if wanted, is checked against the same fixtures. |
| `platypus-lab` | That is the experiment repo (depth / room mapping). This is product code that has to pass the PlatypusOne architecture gate. |
| C++ in `platform/` | Deferred. The device's contour pipeline is C++ (PR #29 `Outline2`). The Python service is the reference implementation, and the JSON contract below is the boundary. A C++ port is a follow-up, held to the same fixtures. |

Cross-repo changes in this slice: **none.** The resolver consumes ShadowScan's
existing JSON formats and produces Mesh2CAD PR #16's existing sketch-asset
schema 1.1 without modifying it.

## 2. Input contract

All three accepted formats are in **image pixels** (x right, y down, origin at
the top-left pixel corner). The proposal records the format, the SHA-256 of the
input bytes, and every input field it does not use (`input.carried`).

| format | source | scale | notes |
|---|---|---|---|
| `shadowscan-outline` | Tab5 ShadowScan applet; PlatypusOne `OutlineExport` (PR #29) | `scale_mm_per_unit` | provenance of the scale is not in the file; recorded as `UNKNOWN` |
| `shadowscan-scan` (SketchJson v1) | ShadowScan Mobile | `mmSketch.imageTransform.unitsPerPixel` when `units == "MM"` | `raw.outer` and `raw.inner` |
| `platypus.contour_observation/1` | native (Engineering Scout) | `calibration.mm_per_px` with `provenance` and `method` | the form to use going forward |

```json
{
  "schema": "platypus.contour_observation/1",
  "observation_id": "scan-0053",
  "units": "px",
  "loops": [
    {"loop_id": "L0", "role": "outer", "closed": true,  "points": [[x, y], ...]},
    {"loop_id": "L1", "role": "hole",  "closed": true,  "points": [[x, y], ...]}
  ],
  "calibration": {"mm_per_px": 0.1109, "provenance": ["sa-ref-area-px"], "method": "reference square area"},
  "unresolved": [{"name": "print_scale", "reason": "not caliper-verified"}]
}
```

Rules:
- Points are kept exactly as given. Booleans, non-finite values and loops with
  fewer than 3 points are rejected. Nothing is resampled or smoothed in the
  stored evidence.
- A missing scale is not an error. The proposal stays in pixels and lists
  `input.calibration` under `unsupported` (*insufficient calibration*). Export
  refuses.
- The resolver never derives or corrects a scale. Physical tolerances are
  `tolerance_px × mm_per_px`, and only when the input is calibrated.
- Loop roles, when absent, come from area and containment: the largest loop is
  outer, loops inside it are holes, and anything else is `unknown`.

## 3. Algorithm

The objective throughout is the **orthogonal distance from each inlier evidence
point to its geometry**. The tolerance `tolerance_px` (default 2 px) is the
*evidence band*. It must be set from the contour source's noise. The benchmark
shows what happens when it isn't (section 7).

1. **Outliers first.** A point is an outlier if it lies more than
   `outlier_factor × tol` (6 px) off a line through its neighbours i±2..i±5,
   over three passes. Outliers are flagged and excluded from fits, never
   deleted.
2. **Gaps.** Spacing between consecutive inliers greater than
   `max(gap_factor × median spacing, gap_min_px)` is a gap. Nothing is fitted
   across a gap.
3. **Whole-circle test.** A closed loop is a circle if one geometric circle
   (Kåsa seed, then Gauss-Newton) fits within the band with at least 300° of
   angular coverage.
4. **Segmentation.** Douglas-Peucker breakpoints (ε = tol) become candidate
   cuts. An optimal DAG chooses pieces: a line costs 1 and an arc 1.15, and a
   piece is allowed only if it *fits*, meaning max residual ≤ tol and RMS ≤
   tol/2. Adjacent pieces are then merged where the merged piece still fits,
   including across the start of a closed loop. Arcs need ≥ 20° of span and ≥ 6
   points, and their radius must stay below 2× the loop size.
5. **Freeform.** Pieces that don't fit, runs of ≥ 3 tiny pieces, smooth runs
   with a curvature reversal or chained arc-arc blends, and runs of short pieces
   whose turns aren't near 90° all become **freeform**. Freeform keeps its
   points, gets no analytic geometry, and cannot be accepted for export.
6. **Competing interpretations.** At corners the resolver also tests *sharp
   corner*, *chamfer line* and *fillet arc*. If more than one fits, the
   primitive becomes a **question** that lists the alternatives.
7. **Constraint hypotheses.** For each relationship it starts from the
   unconstrained fits and fits the constrained model to the same points:
   - parallel / perpendicular: line pairs within ±12° of the relation. The
     joint direction is the eigenvector of the summed (parallel) or differenced
     (perpendicular) scatter matrices.
   - coincident: the junction vertex of neighbouring primitives. For a
     near-tangent line-arc pair, it uses the tangency gap, since the blend point
     along a tangent curve is not measurable.
   - collinear: neighbours across a gap, or across a corner flatter than 8°.
   - tangent: the least-squares circle *within* the tangent family. For a
     fillet between two lines, the radius is free and the centre sits on the
     offset-line intersection. For a semicircle between parallel lines, the
     radius is half the spacing. Comparing against the free fit pushed onto
     tangency would reject real fillets.
   - equal radius: arc and circle pairs within 25%, refit with a shared radius.
   - concentric: centres within 2× tol, refit with a shared centre.
8. **Decision** (`constraints.decide`), using the constrained fit's max and
   RMS residual:

   | condition | decision |
   |---|---|
   | max ≤ tol, RMS ≤ tol/2, and constrained RMS ≤ max(1.25 × free RMS, free RMS + 0.05 tol) | **proposed** |
   | inside the band, but RMS inflated beyond that | **question** (`evidence_distinguishes`): many points can separate radii 12 vs 14 px that a band test alone would let through |
   | max ≤ `question_factor` × tol (4 px) | **question**: plausible intent that moves geometry beyond the evidence |
   | otherwise | **rejected_by_evidence**: accepting it needs a reviewer override note |

   Decisions within 20–25% of a threshold carry `near_threshold`.
9. **Joint solve** (`solve.py`). Only *proposed* constraints are applied
   automatically. Line directions are grouped by parallel/perpendicular
   relations, using a union-find with parity on a shared base direction.
   Equal-radius and concentric groups are refitted. Tangent arcs are rebuilt.
   Collinear pieces share one line. Vertices come from coincident junctions.
   Every entity reports its discrepancy (max and RMS, in px and mm) against its
   own evidence. Constraints that pass one at a time can still overshoot
   together; any entity then beyond the band becomes a `joint_excess`
   question.
10. **Closed profile** only if every junction in a loop is closed by an applied
    coincident or collinear constraint. Otherwise the loop is `unsupported` for
    export, and the reason names the open junction.

### Parameters

Every threshold lives in `contract.Params` and is written into each proposal.
The defaults were set on the **dev** fixtures only, then frozen
(`services/sketch_intent/bench/FREEZE.json`, committed before any held-out run).

| param | default | meaning |
|---|---|---|
| `tolerance_px` | 2.0 | evidence band; must cover about 3.3σ of the contour noise |
| `question_factor` | 2.0 | beyond the band, up to this × tol → question |
| `rms_inflation` | 1.25 | constrained/unconstrained RMS ratio still "proposed" |
| `outlier_factor`, `outlier_window` | 3.0, 5 | outlier rule |
| `gap_factor`, `gap_min_px` | 6, 6 px | gap rule |
| `angle_window_deg` | 12 | parallel/perpendicular hypotheses tested within this |
| `min_arc_span_deg`, `min_arc_points`, `max_radius_factor` | 20°, 6, 2 | arc admissibility |
| `min_circle_span_deg` | 300° | whole-loop circle |
| `equal_radius_rel`, `concentric_factor` | 0.25, 1 | candidate generation (not acceptance) |
| `min_corner_angle_deg` | 8° | flatter junctions are collinear candidates |

**Residuals are fit diagnostics, not accuracy.** A 0.1 mm residual says the
geometry follows the contour. It says nothing about whether the contour
follows the part: segmentation, lens distortion, perspective and scale error
are outside the resolver.

## 4. Proposal (`platypus.sketch_proposal/1`)

| field | content |
|---|---|
| `proposal_id` | sha256(input sha256 + params + `RESOLVER_VERSION`)[:20]; the proposal is a pure function of these |
| `input` | format, sha256, source ids, coordinate frame, scale, calibration provenance, `unresolved`, `carried` |
| `parameters` | every `Params` value plus `tolerance_mm` (null when uncalibrated) |
| `evidence` | per loop: original `points_px` unmodified, gaps, outlier indices |
| `primitives` | id, kind, point indices, fit (px and mm), residual max/RMS (px and mm), alternatives, decision |
| `constraints` | id, type, entities, decision, measured values, correction magnitude, constrained-fit residuals, reason, flags |
| `questions` | competing interpretations, constraint questions, joint excess, each with options |
| `ambiguities` | competing interpretations, near-threshold decisions, solver conflicts, joint excess |
| `unsupported` | freeform evidence, open profiles, insufficient calibration |
| `proposed_geometry` | geometry with only *proposed* constraints applied, entity discrepancies, closed-profile state. Labelled "inferred intent, not measurement" |
| `review` | `state` (pending, partial, complete), `decisions`, `undecided` |

## 5. Review and export

```sh
python3 -m sketch_intent resolve in.json -o proposal.json --svg proposal.svg
python3 -m sketch_intent review proposal.json -o reviewed.json --by NAME --accept-proposed [--accept ID ...] [--reject ID ...]
python3 -m sketch_intent export reviewed.json --input in.json -o part.sketch.json [--mesh2cad PATH --dxf part.dxf]
python3 -m sketch_intent overlay reviewed.json --input in.json -o accepted.svg
```

The review interface is the CLI plus the overlay; there is no separate app.
Review rules:
- `--accept-proposed` accepts only what the resolver *proposed*. Questions stay
  undecided until someone answers them.
- Unsupported items (freeform) cannot be accepted.
- Accepting a `rejected_by_evidence` item requires `--override-note`, which is
  stored with the decision.
- Each review writes a new file (it never overwrites) carrying
  `parent_sha256`, the reviewer, a timestamp, and the resolver's own decision
  next to the human one.

Export (`export.py`):
- It re-reads the original input. It refuses if the input bytes no longer hash
  to the proposal's input, or if re-running the resolver gives a different
  `proposal_id` (code or parameters changed).
- It applies only accepted constraints and re-solves.
- A loop is exported only if all its primitives are accepted and it closes.
  Other loops are skipped and listed, never approximated.
- Uncalibrated input → refuses (no mm geometry without a scale).
- Output is a Mesh2CAD `SketchAsset` dict (schema 1.1): circles go to
  `circles`; other loops become `profiles` of `line` and `arc` segments, with
  role `outline` or `cutout`. Coordinates are mm, Y-up, with the origin at the
  outer loop's bounding-box lower-left. Segment ends are made exactly continuous.
  `provenance` carries the proposal id, input sha256, source ids, calibration,
  the accepted items, the reviewers, the skipped loops, and the note "reviewed
  design intent… not measured truth".
- With `--mesh2cad`, the asset is validated by `mra.sketch_assets.SketchAsset.from_dict`
  and written by Mesh2CAD's own `export_dxf` and `export_svg`. There is one
  exporter, and it isn't duplicated here.

## 6. Sharing with ShadowScan and Mesh2CAD

- **ShadowScan Mobile**: produce `shadowscan-scan` (already done) or
  `contour_observation/1`, and run the resolver off-device, or port it later
  against `services/sketch_intent/tests/fixtures.py`. ShadowScan's own
  `LineRefinement` (TLS lines, bowed-residual arc test) is consistent with
  steps 4–5. Its outputs can be compared fixture by fixture.
- **Mesh2CAD**: the resolver is a *producer* for `mra.sketch_assets` (PR #16).
  It doesn't change that library. If PR #16's schema changes, `export.py` and
  the pinned CI checkout change with it.
- **Engineering Scout (PlatypusOne)**: the Scout session pipeline (PR #29
  `Outline2`, `OutlineExport`) should write `contour_observation/1` with the
  observation id and calibration provenance it already records. The resolver
  then runs on the Linux side. No UI code owns the CAD semantics.

## 7. Known limitations (from the benchmark)

These are reported in detail, with numbers, in the
[benchmark](SKETCH_INTENT_BENCHMARK.md#failure-cases-generated):

1. **The tolerance must match the contour noise.** At the default 2 px band
   with σ ≥ 0.75 px of contour noise, segmentation falls apart, and some
   perpendiculars on 2° and 6° skewed shapes get **proposed**. Runtime also
   grows to seconds. With the band set to cover the noise (about 3.5σ), the
   6° skew is never squared, while a 2° skew becomes indistinguishable from 90°
   once the band exceeds the skew's displacement. That is what a band means,
   but a reviewer has to see it. Follow-up: estimate contour noise from the
   evidence and flag a band that is too tight. This is a diagnostic, never an
   automatic retune.
2. **Angular constraints on short segments are weakly determined.** On the
   real washer, short spurious line pieces got a perpendicular proposal about
   5° off, inside the band because the pieces are short. Follow-up: a
   `weakly_determined` flag from the angular uncertainty (tol / length), and
   no automatic angle proposals below a minimum length.
3. **Real round parts may not be one circle within the band.** On scan-0053
   the outer edge fragments into arcs and lines. Export correctly refuses (not
   closed). A reviewer currently cannot say "this is a circle" in one step.
4. **Pairwise hypotheses do not scale.** On a 52-loop PCB render there were
   about a thousand equal-radius hypotheses. Follow-up: cluster radii into
   groups and propose one group constraint each.
5. **Small fillets and chamfers are indistinguishable** at a 2 px band. With
   r ≈ 11 px, fillets come out as chamfer lines plus a competing-interpretation
   question. That is correct behaviour, but it costs review time.
6. Not yet implemented from issue #37: dominant part frame and
   horizontal/vertical constraints, bilateral symmetry, patterns (repeated
   holes as a group), a named `slot` primitive (a slot is currently
   line-arc-line-arc with tangency), ellipses (correctly *not* invented, but
   reported as freeform or arcs), and the symmetric-gasket fixture.
7. Python reference only; no C++ port, no Scout integration yet.

## 8. Physical validation still required

Nothing here is physical evidence. Before any claim about real parts, issue
#37's physical gate needs:

- [ ] several **simple planar engineering parts** (plate, bracket, gasket,
  L-plate), not only fasteners, captured with the existing webcam and reference
  workflow, with raw frames preserved (as in PR #39's evidence layout);
- [ ] **caliper truth** for a few critical dimensions per part (hole diameter,
  hole spacing, overall length and width, slot width), recorded with who
  measured and when;
- [ ] for each part: raw silhouette overlay, proposed intent overlay, and
  accepted overlay (this service produces all three);
- [ ] contour noise σ measured for that capture path, and `tolerance_px` set
  from it *before* looking at results;
- [ ] **time to confirm or fix** the proposed sketch versus manually sketching
  the same part in Fusion, timed the same way for both;
- [ ] the print scale of the reference verified with calipers (scan-0053 lists
  it as UNRESOLVED);
- [ ] decision: continue only if the reviewed sketch is Fusion-ready and faster
  than manual reconstruction.

The single real capture in the benchmark (scan-0053) is a smoke test of the
path on real pixels, not this gate.

## 9. Reproducing the benchmark

```sh
PYTHONPATH=services/sketch_intent python3 services/sketch_intent/bench/benchmark.py \
    --shadowscan ../shadowscan-mobile --washer-ref <ref carrying PR #39 evidence> \
    --out docs/architecture/SKETCH_INTENT_BENCHMARK.md
```

The committed report was generated with ShadowScan Mobile at `04a868c` and the
PR #39 evidence branch. Without those, the external sections say "Not run".
`--quick` is the CI smoke variant.
