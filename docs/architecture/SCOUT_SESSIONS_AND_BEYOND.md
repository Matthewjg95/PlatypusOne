# Scout sessions — and where they lead

Written 2026-09-30, alongside the first session implementation (PR #29). It
records the design of sessions, the extension mechanism that lets them grow
past fasteners, and an honest path from today's calibrated silhouette to 3D
and to a tool that *helps* with engineering work rather than only measuring.

Everything here keeps the [Engineering Observation contract](ENGINEERING_OBSERVATION_CONTRACT.md):
observed / derived / inferred / unresolved stay separate, every inference
carries confidence and provenance, and nothing overwrites evidence.

## 1. What a session is

A capture answers "what does this one frame show". A **session** answers
"what do we know about this object", by grouping repeated captures of it:

- Each capture stays an independent, append-only observation record under
  `observations/`. The session only **references** them.
- The session runs **guidance**: what to capture next, until the model is
  satisfied or the operator finishes.
- Aggregation is **repeat statistics of the same measurand** (mean, spread,
  count, with provenance to each contributing claim). Combining *different*
  views into new geometry is fusion, which the contract defers; it arrives
  deliberately, per §4, not as a side effect.
- On finish, the session writes CAD artifacts beside `session.json`.

Guidance has three stages: **get a measurement** (relay the analyzer's
refusals — no square, no part), **make it repeatable** (≥ 3 captures within
tolerance; shifting and rotating the part between them also exercises the
rotation-invariant width), and **complete the picture** (list what is still
unknown and the view that would resolve it — marking honestly what this
build cannot yet analyze).

## 2. Object profiles — the extension slot

Fasteners are the first object class, not the design. What differs between
classes is data, so it lives in an **object profile**:

| Profile field | Meaning |
|---|---|
| measurands | what repeatability is judged on (length/width for a screw; outline + hole pattern for a PCB) |
| open questions | what a top-down silhouette cannot answer, and why |
| resolving views | the capture that would answer each question (side-on, end-on, backlit, ToF) |
| analyzers present | which of those this build can actually process — the honesty flag |
| CAD template | what the finished session exports (outline, extrude, revolve…) |

The first implementation ships the **fastener** profile. The table below is
the roadmap for the next ones, ordered by value per unit of new work.

### PCB

The strongest next case: opaque board, through-holes that let light through,
and a real CAD need (enclosures, adapter boards, replacement mounts).

- **Today's pipeline already gets:** board outline and mounting holes from a
  backlit or high-contrast top-down silhouette. Holes are exactly the
  outline's holes.
- **Needs:** hole-pattern recognition (§5), thickness (side view; 1.6 mm is
  the default to *suggest*, never assume), component heights (side view or
  ToF — keep-out volumes for an enclosure).
- **CAD:** outline + holes as DXF → Fusion sketch → extrude by thickness;
  the same DXF imports into KiCad as `Edge.Cuts` for an adapter board.
- **Lighting:** the LED tracing pad (backlight) makes PCBs near-trivial —
  it silhouettes the board and shows every hole, and removes the shadow bias
  measured on 2026-09-29.

### Flat parts — brackets, plates, gaskets, laser-cut parts

- **Today:** outline + holes + slots from one silhouette.
- **Needs:** thickness (side view or operator entry), slot recognition
  (stadium shapes), bend-line detection for sheet metal (later).
- **CAD:** DXF is directly a laser/waterjet cut file; extrude by thickness
  for a solid.

### Turned parts — shafts, spacers, bushings, pins

- **Key insight:** a round part lying on its side shows its **revolve
  profile** in a top-down silhouette. Half of that outline, revolved about
  its axis, *is* the 3D model — the cheapest silhouette-to-3D path there is.
- **Needs:** axis detection (principal axis — already computed), symmetry
  check (the two halves must agree, which is itself evidence), parallax
  correction for the part's height above the paper (§3).
- **CAD:** revolve sketch.

### Extrusions — T-slot, channels, tubing

- **Today:** an end-on silhouette is the cross-section.
- **Needs:** end-on capture guidance, profile matching to standards
  (2020 / 2040 / 3030), length from a second view.
- **CAD:** extrude sketch; or "this is 2020 T-slot" as an inferred claim.

### Printed and moulded parts, enclosures

- Genuinely 3D; silhouettes alone underdetermine them. They come with §3
  tier 2–3, and mesh2cad's design-intent recovery.

## 3. The path to 3D, in honest tiers

| Tier | What it measures | What it needs | Limits |
|---|---|---|---|
| **0 — today** | calibrated top-down silhouette at the paper plane | camera + printed reference | anything above the paper reads larger (parallax); low light adds a shadow halo |
| **1 — 2.5D** | extrude and revolve parts: silhouette + one height | a side-view analyzer with its own in-frame reference; pose from the profile ("lying flat", "on its side") | only parts that *are* extrusions or revolutions |
| **2 — multi-view silhouettes** | visual hull of any rigid part | turntable (STM32-driven stepper — the MCU domain's deterministic I/O), known view angles, camera intrinsics from the calibration sheet | cannot see concavities, threads or internal features |
| **3 — depth** | real surface geometry | laser-line triangulation (a line laser + the same camera), VL53L8CX ToF for coarse height/keep-outs, photometric stereo for detail | cost and calibration effort grow per step |

Two things tier 1 buys immediately, beyond new shapes:

- **Parallax correction.** A part's top surface sits closer to the camera
  than the reference square. Once its height is known (side view, or a
  datasheet value the operator confirms), its magnification can be corrected.
  This is the first honest fix for the head-width bias measured on the bench.
- **Shadow separation.** Two differently lit captures of the same pose
  disagree only where shadows are; that disagreement is evidence of which
  edge is real.

## 4. From measuring to helping

"Getting actual help from the program" is a ladder. Each rung is useful on
its own and keeps inference visibly separate from measurement.

1. **Guidance** — knows what evidence it still lacks and asks for it. *This PR.*
2. **Standards matching** — measurements snapped to what engineers actually
   use, as INFERRED claims with the measured value beside them:
   hole → M3 clearance (ISO 273, 3.4 mm medium); thread → metric coarse vs
   fine; sheet → gauge; hole pattern → known footprint
   (Raspberry Pi 58 × 49 mm, Arduino UNO, VESA 75/100); profile → 2020 T-slot.
3. **Design intent** — coordinates become constraints: "four holes on a
   58 × 49 mm rectangle, 3.5 mm from the edges, symmetric". This is what
   turns a traced outline into a sketch someone can edit. mesh2cad already
   targets design-intent recovery; sessions feed it evidence with provenance.
4. **Parametric CAD** — instead of dumb geometry, emit a Fusion 360 script
   (the Fusion API is Python) that builds sketch + constraints + extrude /
   revolve + hole features. DXF is the floor; a parametric script is the
   ceiling, and it is where PlatypusOne meets the Autodesk contest.
5. **Finding and mating parts** — look up the connector or fastener
   (DigiKey's API: the Dream Lab sponsor), propose the mating part or the
   bracket that mounts this board, with clearances and tolerances stated.

Rules that hold at every rung:

- Suggestions are INFERRED, with confidence, provenance and method — never
  silently substituted for the measurement.
- `human_review` is where the engineer accepts, rejects or corrects; a
  correction is a new record, not an edit.
- A question the build cannot answer is shown as open, with the view or
  sensor that would answer it. The system knowing what it does not know is
  the feature.

## 5. What each step needs

| Need | For | Status |
|---|---|---|
| Object profiles in the session model | every new class | this PR (fastener) |
| Outline export (Outline Forge JSON, DXF) | CAD handoff | this PR |
| Backlight stage (LED tracing pad) | PCBs, clean edges, no shadow bias | ~$20 purchase |
| Fixed camera rig | repeatable scale, tier 1 | 3D-printed, in progress |
| Side-view analyzer with in-frame reference | tier 1, parallax correction | next |
| Camera intrinsics from the calibration sheet | tier 2, lens distortion | next-next |
| Turntable (stepper on the STM32) | tier 2, both compute domains | future |
| Hole-pattern + standards tables | helping rung 2 | future |
| Fusion script emitter | helping rung 4 | future |

## 6. Near-term order

1. Sessions with the fastener profile and CAD export — *this PR*.
2. Data before tuning: sessions under several lighting setups, including the
   backlight, to quantify the shadow bias (no analyzer changes until then).
3. PCB profile: backlit outline + holes → DXF → KiCad/Fusion round trip.
4. Side-view analyzer → tier 1 (revolve/extrude) and parallax correction.
5. Hole-pattern recognition → the first "helping" inference beyond fasteners.
