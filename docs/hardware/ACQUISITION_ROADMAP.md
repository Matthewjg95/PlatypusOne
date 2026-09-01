# Platypus One — Hardware Acquisition & Build Roadmap

Starting position (**2026-08-04**): zero hardware in hand. All dates anchored
to the AU 2027 contest timeline (see
[contest snapshot](../contest/autodesk-au2027-contest-snapshot.md)). Software
continues on the host simulator throughout, so hardware never blocks code.

```mermaid
gantt
    dateFormat  YYYY-MM-DD
    title Platypus One — road to Dec 20 submission
    section Contest gates
    Hardware application window   :crit, 2026-08-04, 2026-09-07
    Recipients announced          :milestone, 2026-09-25, 0d
    Submission deadline           :milestone, crit, 2026-12-20, 0d
    section Hardware
    Fusion enclosure first pass   :2026-08-05, 12d
    Early de-risk parts (display, IMU) :2026-08-10, 10d
    Main parts order + UNO Q      :2026-09-25, 10d
    Bring-up (checklists 1–7)     :2026-10-05, 21d
    Carrier PCB spin 1 (PCBWay)   :2026-10-19, 14d
    Enclosure print (PCBWay)      :2026-10-26, 14d
    PCB/enclosure respin buffer   :2026-11-10, 14d
    section DFM gates
    DFM-0 architecture review     :crit, 2026-09-25, 8d
    DFM-1 mechanical interfaces   :milestone, 2026-10-12, 0d
    DFM-2 manufacturing release   :milestone, crit, 2026-10-18, 0d
    DFM-3 first-article review    :2026-11-03, 5d
    DFM-4 production readiness    :milestone, crit, 2026-11-23, 0d
    section Software
    Display+camera+IMU drivers    :2026-09-01, 45d
    ShadowScan + measurement apps :2026-10-15, 40d
    section Submission
    Integration soak + metrics    :2026-11-24, 14d
    Video, photos, writeup        :2026-12-01, 15d
    Submit (buffer before deadline):crit, 2026-12-16, 4d
```

## DFM gates — evidence required to advance

These are schedule gates, not advisory reviews. A gate closes only when its
artifacts are committed or linked from the repository. An unresolved
critical-to-function item blocks the associated PCB/enclosure release.

| Gate | Window | Exit evidence |
|---|---|---|
| **DFM-0 — architecture** | Sep 25–Oct 2 | Selected processes and suppliers; PCB stack-up/assembly class; enclosure process; standard fasteners/connectors; optical, RF, thermal, battery-service, and cable-access constraints; costly/manual operations identified |
| **DFM-1 — mechanical interfaces** | Close by Oct 12 | Board/enclosure datums; connector reach and strain relief; camera/ToF/illumination optical axes and keep-outs; antenna RF window; tolerance stack; thermal path; programming, test-point, and assembly-tool access |
| **DFM-2 — manufacturing release** | Close Oct 16–18, before the Oct 19 order | Fusion Electronics DRC; footprint and pin-1 audit; fab-rule, annular-ring, solder-mask, paste, pick-and-place, assembly-side, and BOM-availability checks; enclosure wall/minimum-feature/tool-radius/support review; fabrication drawing and export/re-import check; PCBWay manufacturability/quote review |
| **DFM-3 — first article** | Nov 3–7, before the Nov 10 respin gate | Critical dimensions measured against drawing; assembly sequence/time; connector strain; sensor alignment; thermal and power checks; test access; fit and serviceability review; defect, bodge, and rework log with Rev-B disposition |
| **DFM-4 — production readiness** | Close Nov 20–23, before hardware freeze | As-built BOM and drawings reconciled; assembly/calibration/inspection instructions; torque/adhesive/fastener controls; risky manual operations identified; 750-unit scaling and cost review; no unresolved critical manufacturing issue |

For the contest narrative, preserve Fusion interference screenshots, tolerance
callouts, manufacturing exports, first-article photographs and measurements,
and the issue-to-correction trail. DFM is both risk control and visible evidence
that PlatypusOne was engineered for manufacture.

## Phase 0 — NOW → Sep 7 (hardware application) 🔴 critical path

> **Deadline extended from Aug 23 to Sep 7, 2026.** Detailed working document
> with draft copy, BOM excerpt, and form answers:
> [HARDWARE_APPLICATION_CHECKLIST.md](../contest/HARDWARE_APPLICATION_CHECKLIST.md).

Goal: submit a strong free-hardware application (UNO Q 4G + $300 PCBWay + Fusion license).

- [ ] Create Hackster account; register as contest participant
- [ ] Start the Hackster project page: name, description, cover image
      (**original artwork** — the concept sheets in `docs/media/` are
      AI-generated references and must not stand in for original work)
- [ ] Install Autodesk Fusion (free personal license); model first-pass
      handheld enclosure around the UNO Q footprint and a **display envelope**
      — the panel is deliberately unfrozen per
      [ADR-0001](../adr/0001-dynamic-linked-prototype-display.md); export .f3d
- [ ] Publish the curated **Hardware Request BOM** from [the application checklist](../contest/HARDWARE_APPLICATION_CHECKLIST.md#4-bom-excerpt-for-the-project-page-agent) to the project page — must list the UNO Q and Fusion. Do not paste the master planning BOM or its received-research rows; see [BOM scope layers](BOM_SCOPE_LAYERS.md)
- [ ] Answer all application-form questions; submit via "Apply for hardware"
      tab **no later than Sep 4** (3-day margin)
- [x] Confirm on the live page whether the Sep 18 announcement date moved with
      the extension — ✅ verified 2026-08-30: **moved to Sep 25, 2026 5:00 PM
      PDT**. All Sep 18 anchors below are re-dated; bring-up now overlaps the
      Oct 19 PCB spin-1 order, so breadboard validation of carrier-PCB-relevant
      sensors must front-load into the first two weeks after parts arrive
- [ ] Optional de-risk buy (~$25): BNO055 IMU, so driver work can start on a
      spare SBC/dev board before the UNO Q arrives. The rev A display de-risk
      buy is **cancelled** — a linked prototype display is already in hand

## Phase 1 — Sep 8 → Sep 25 (waiting on announcement)

- [ ] Refine Fusion model: button placement, camera/ToF windows, battery bay
- [ ] Start Fusion **Electronics**: carrier-board schematic (I²C hub, buttons,
      power) — targets the Best Fusion Electronics Design prize
- [ ] Software: **linked-display driver** against the
      [presentation link](../protocols/presentation.md) (the panel is unfrozen,
      so no integrated-panel driver is written yet) + IMU ISensor driver
      written against interfaces; STM32 firmware for the MCU bridge protocol
- [ ] Draft the project story on Hackster as work happens (docs = 20 pts)

## Phase 2 — Sep 25 → mid-Oct (parts + bring-up)

Decision point Sep 25 (verified on the live page 2026-08-30; was Sep 18):
- **Selected:** hardware + credit incoming; reconcile the [Contest Core scope](../contest/PLATYPUSONE_CORE_SCOPE.md) against the master [BOM](BOM.md), then order only selected Core items the same week. Received-research rows R1–R4 are excluded unless explicitly promoted. Every ⚖ DECISION line must close first.
- **Not selected:** buy UNO Q retail (~$59) immediately; self-fund a reduced
  enclosure budget; scope unchanged otherwise.

- [ ] Execute [test checklists](TEST_CHECKLISTS.md) 1–2 (boot, PlatypusOS, MCU bridge)
- [ ] Checklists 3–7 as parts land (display, camera, sensors, power, audio)
- [ ] Breadboard-validate every sensor **before** freezing the carrier PCB
- [ ] Close **DFM-0** by Oct 2 and **DFM-1** by Oct 12; unresolved critical
      interface or manufacturing assumptions remain release blockers
- [ ] Close **DFM-2** by Oct 18 before authorizing the Oct 19 fabrication order

## Phase 3 — mid-Oct → mid-Nov (PCBWay + integration)

- [ ] Order carrier PCB spin 1 + enclosure from PCBWay (~2-week turn incl. shipping)
- [ ] Checklist 8–9 on arrival; log defects
- [ ] Execute **DFM-3 first-article review Nov 3–7** and feed every material
      defect, bodge, fit issue, or assembly burden into the Nov 10 respin decision
- [ ] **Respin decision gate: Nov 10** — last date a second PCB/enclosure
      order safely lands before the freeze
- [ ] Apps: ShadowScan capture→mesh→export path; measurement app calibrated

## Phase 4 — mid-Nov → Dec 20 (freeze, document, submit)

- [ ] Close **DFM-4 production-readiness review by Nov 23**
- [ ] **Hardware freeze Nov 24** — after this, software and documentation only
- [ ] Checklist 10 integration soak; capture accuracy metrics
- [ ] Shoot submission video + photos (device in action, in hand)
- [ ] Final BOM reconciliation; schematics exported from Fusion; .f3d attached;
      code repo linked/licensed cleanly (cite all third-party licenses)
- [ ] **Submit Dec 16–18** — never the deadline day

## Standing risks

| Risk | Mitigation |
|---|---|
| Not selected for free hardware (announced Sep 25) | Budget reserves $430 self-fund path; scope trims pre-planned (FDM enclosure, drop haptics) |
| UNO Q display path harder than expected (SPI from Linux vs MCU) | Fall back to driving panel from STM32 side over the bridge; renderer already targets dumb-framebuffer present() |
| PCBWay turnaround eats December | Spin-1 order by Oct 19; respin gate Nov 10; enclosure printable locally as emergency fallback |
| Single point of failure: one UNO Q | Handle with care; no live rewiring; consider second board if budget allows after Sep 25 |
| Docs crunch at deadline | Write the Hackster story continuously from Phase 1 (it's also 50/100 rubric points) |
