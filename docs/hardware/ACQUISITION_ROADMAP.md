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
    Hardware application window   :crit, 2026-08-04, 2026-08-23
    Recipients announced          :milestone, 2026-09-18, 0d
    Submission deadline           :milestone, crit, 2026-12-20, 0d
    section Hardware
    Fusion enclosure first pass   :2026-08-05, 12d
    Early de-risk parts (display, IMU) :2026-08-10, 10d
    Main parts order + UNO Q      :2026-09-18, 10d
    Bring-up (checklists 1–7)     :2026-09-28, 21d
    Carrier PCB spin 1 (PCBWay)   :2026-10-19, 14d
    Enclosure print (PCBWay)      :2026-10-26, 14d
    PCB/enclosure respin buffer   :2026-11-10, 14d
    section Software
    Display+camera+IMU drivers    :2026-09-01, 45d
    ShadowScan + measurement apps :2026-10-15, 40d
    section Submission
    Integration soak + metrics    :2026-11-24, 14d
    Video, photos, writeup        :2026-12-01, 15d
    Submit (buffer before deadline):crit, 2026-12-16, 4d
```

## Phase 0 — NOW → Aug 23 (hardware application) 🔴 critical path

Goal: submit a strong free-hardware application (UNO Q 4G + $300 PCBWay + Fusion license).

- [ ] Create Hackster account; register as contest participant
- [ ] Start the Hackster project page: name, description, cover image
- [ ] Install Autodesk Fusion (free personal license); model first-pass
      handheld enclosure around UNO Q + 3.2" display footprints; export .f3d
- [ ] Publish preliminary BOM (adapt [BOM.md](BOM.md)) to the project page —
      must list the UNO Q and Fusion
- [ ] Answer all application-form questions; submit via "Apply for hardware"
      tab **no later than Aug 21** (2-day margin)
- [ ] Optional de-risk buys (~$45): ILI9341 touch display + BNO055 IMU, so
      driver work can start on a spare SBC/dev board before the UNO Q arrives

## Phase 1 — Aug 24 → Sep 18 (waiting on announcement)

- [ ] Refine Fusion model: button placement, camera/ToF windows, battery bay
- [ ] Start Fusion **Electronics**: carrier-board schematic (I²C hub, buttons,
      power) — targets the Best Fusion Electronics Design prize
- [ ] Software: display driver (SPI ILI9341) + IMU ISensor driver written
      against interfaces; STM32 firmware for the MCU bridge protocol
- [ ] Draft the project story on Hackster as work happens (docs = 20 pts)

## Phase 2 — Sep 18 → mid-Oct (parts + bring-up)

Decision point Sep 18:
- **Selected:** hardware + credit incoming; order BOM items 2–15, 18–19 same week.
- **Not selected:** buy UNO Q retail (~$59) immediately; self-fund a reduced
  enclosure budget; scope unchanged otherwise.

- [ ] Execute [test checklists](TEST_CHECKLISTS.md) 1–2 (boot, PlatypusOS, MCU bridge)
- [ ] Checklists 3–7 as parts land (display, camera, sensors, power, audio)
- [ ] Breadboard-validate every sensor **before** freezing the carrier PCB

## Phase 3 — mid-Oct → mid-Nov (PCBWay + integration)

- [ ] Order carrier PCB spin 1 + enclosure from PCBWay (~2-week turn incl. shipping)
- [ ] Checklist 8–9 on arrival; log defects
- [ ] **Respin decision gate: Nov 10** — last date a second PCB/enclosure
      order safely lands before the freeze
- [ ] Apps: ShadowScan capture→mesh→export path; measurement app calibrated

## Phase 4 — mid-Nov → Dec 20 (freeze, document, submit)

- [ ] **Hardware freeze Nov 24** — after this, software and documentation only
- [ ] Checklist 10 integration soak; capture accuracy metrics
- [ ] Shoot submission video + photos (device in action, in hand)
- [ ] Final BOM reconciliation; schematics exported from Fusion; .f3d attached;
      code repo linked/licensed cleanly (cite all third-party licenses)
- [ ] **Submit Dec 16–18** — never the deadline day

## Standing risks

| Risk | Mitigation |
|---|---|
| Not selected for free hardware (announced Sep 18) | Budget reserves $430 self-fund path; scope trims pre-planned (FDM enclosure, drop haptics) |
| UNO Q display path harder than expected (SPI from Linux vs MCU) | Fall back to driving panel from STM32 side over the bridge; renderer already targets dumb-framebuffer present() |
| PCBWay turnaround eats December | Spin-1 order by Oct 19; respin gate Nov 10; enclosure printable locally as emergency fallback |
| Single point of failure: one UNO Q | Handle with care; no live rewiring; consider second board if budget allows after Sep 18 |
| Docs crunch at deadline | Write the Hackster story continuously from Phase 1 (it's also 50/100 rubric points) |
