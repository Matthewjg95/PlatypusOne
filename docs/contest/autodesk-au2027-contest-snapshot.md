# Contest Snapshot — "Build the Autodesk University 2027 Product"

Snapshot taken **2026-08-04** from
https://www.hackster.io/contests/autodesk-university-2027-product (overview +
rules tabs). Verify against the live page before acting on deadlines; Hackster
notes end dates may change.

Host: Hackster.io (Avnet). Sponsors: **Autodesk, Arduino, Qualcomm, PCBWay**.
Prize pool: $4,000+.

## What to build

A **handheld/wearable smart accessory** ("AU 2027 Product") designed in
**Autodesk Fusion**, built on the **Arduino UNO Q**, with a **PCBWay-made
enclosure** ($300 credit for hardware recipients). It must:

- Enhance how you interact with yourself, your environment, or other people
- Be handheld and legal in carry-on luggage
- Be useful beyond the conference — something any Autodesk customer could use
- May use injection molding, 3D printing, CNC, PCBs, off-the-shelf parts

Winning concept gets manufactured (~750 units) for AU 2027. Prior-year devices:
air quality meter, digital badge, macropad, **multi-measure device (2025:
angle/distance/level/color)** — Platypus One (engineering multi-tool handheld
with scan/measure/inspect apps) is squarely in this lineage.

## Timeline (all times PT)

| Date | Milestone |
|---|---|
| Jul 28, 2026 12:00 PM | Competition begins (open now) |
| ~~Aug 23, 2026~~ → **Sep 7, 2026 11:59 PM** | **Free hardware application closes — EXTENDED** |
| Sep 18, 2026 5:00 PM | Hardware recipients announced — ⚠️ may have moved with the extension, unverified |
| **Dec 20, 2026 11:59 PM** | **Project submission closes** |
| Jan 29, 2027 | Winners announced |

> **Update 2026-08-24:** the hardware application window was extended to
> **Sep 7, 2026** (reported by the owner; confirm against the live page). The
> Sep 18 announcement and Dec 20 submission dates are carried over from the
> Aug 4 snapshot and have **not** been re-verified since the extension — if the
> announcement slipped, the bring-up window in
> [ACQUISITION_ROADMAP.md](../hardware/ACQUISITION_ROADMAP.md) compresses.
> Working checklist: [HARDWARE_APPLICATION_CHECKLIST.md](HARDWARE_APPLICATION_CHECKLIST.md).

## Hardware application (deadline Sep 7 — extended)

100 recipients get: Fusion license for the competition + **UNO Q (4G)** +
**up to $300 PCBWay credit**. A further 200 get an UNO Q (4G) only.

Application must include:

- [ ] Link to a **started Hackster project** containing:
  - [ ] Beginning of the **BOM** (must include the UNO Q and Autodesk Fusion)
  - [ ] Your **Fusion design/dataset (.f3d file)** — not final, but shows the plan
- [ ] Answers to all application-form questions
- [ ] Written in English, submitted via the contest page's "Apply for hardware" tab

Accepting hardware = contract to build, document, and **submit by the
deadline** (or return the hardware).

## Final submission deliverables (deadline Dec 20)

- [ ] Project name, short description, cover image
- [ ] **Complete BOM** (hardware, software, tools)
- [ ] Full build instructions ("could a beginner recreate it?")
- [ ] Images throughout the build
- [ ] Resource files: schematics, code, CAD
- [ ] **Autodesk Fusion design/dataset (.f3d) included**
- [ ] **PCBWay enclosure** in the build
- [ ] **Arduino UNO Q** in the build
- [ ] **Video + photos of the prototype in action**
- [ ] Written in English; submitted on Hackster by Dec 20, 11:59 PM PT

## Judging rubric (100 points)

| Criterion | Points | Notes |
|---|---|---|
| Creativity | **30** | Fresh take counts as much as novel idea |
| Project documentation | 20 | Story/instructions, images, video demo |
| Complete BOM | 20 | Hardware, software, tools detailed |
| Schematics | 10 | Drawn in Fusion and/or detailed photos |
| Use of Autodesk Fusion | 10 | |
| Code & contribution | 10 | Working, well-commented code |

Documentation + BOM + schematics = 50 points — half the score is paperwork.
Keep them current as you build, not retrofitted at the end.

## Prizes

| Prize | Value | Award |
|---|---|---|
| Best AU 2027 Product Concept | $1,680 | $1000 + 1-yr Fusion + manufactured for AU 2027 |
| Best Fusion Electronics Design | $1,680 | $1000 + 1-yr Fusion (requires .f3d in submission) |
| Best Use of Arduino UNO Q | $1,000 | $1000 |

## Key rules to remember

- **Solo only** — no teams.
- 18+, citizen/resident of listed countries (USA included).
- Original work; not a prior Hackster contest winner; no third-party
  trademarks/copyrighted material without permission — cite licenses for any
  open-source code used.
- No military/defense/harmful-surveillance applications; export-law compliant.
- Submitted projects become public and cannot be deleted after the deadline;
  sponsors get a broad license to feature the entry.
- One prize per person; judges' decisions final. Governed by Arizona law.
- Contact: contests@hackster.io

## Immediate implications for Platypus One

1. **11 days** to the hardware application (as of 2026-08-24, extended
   deadline Sep 7): need a Hackster project page, a preliminary BOM, and a
   first-pass Fusion .f3d of the handheld enclosure. The BOM is ready
   ([BOM.md](../hardware/BOM.md) rev B); the **.f3d is the critical path** —
   it is the one required deliverable nobody else can produce. Work it from
   [HARDWARE_APPLICATION_CHECKLIST.md](HARDWARE_APPLICATION_CHECKLIST.md).
2. Fusion is a required tool — start the enclosure/electronics design there
   now (free personal license works for the application stage).
3. Plan the build assuming hardware arrives **after Sep 18** — ~13 weeks from
   hardware to submission. Keep the host-sim workflow as the main dev path
   (already in place) so software progress never blocks on parts.
