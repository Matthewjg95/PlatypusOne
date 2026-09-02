# Hardware Application Checklist — due Sep 7, 2026

Working document for the AU 2027 free-hardware application (UNO Q 4G + up to
$300 PCBWay credit + Fusion license). Requirements are from the
[contest snapshot](autodesk-au2027-contest-snapshot.md); the deadline reflects
the **extension from Aug 23 to Sep 7, 2026** reported by the owner.

> **The owner submits. The agent does not.** Nothing in this repository
> attempts a web submission, creates a Hackster account, or uploads a file.
> Everything below marked **[agent]** is prepared here as text to paste or
> attach; everything marked **[owner]** needs a human.

**Target submit date: Sep 4** — three days of margin. Never the deadline day.
As of 2026-08-24 that is **11 days out**.

## 1. Verify before anything else **[owner]**

- [x] Open the live contest page and confirm the extended deadline
      (**Sep 7, 2026, 11:59 PM PT**) in writing on the page itself —
      ✅ verified 2026-08-30: "September 7, 2026 at 11:59 PM PDT"
- [x] Confirm whether the **Sep 18 recipients-announcement** date moved with
      the extension — ✅ verified 2026-08-30: **it moved, to Sep 25, 2026
      5:00 PM PDT**. The downstream plan in
      [ACQUISITION_ROADMAP.md](../hardware/ACQUISITION_ROADMAP.md) is anchored
      to Sep 18 and needs a one-week compression pass
- [x] Confirm the submission deadline is still **Dec 20, 2026** —
      ✅ verified 2026-08-30: "December 20, 2026 at 11:59 PM PST"
- [ ] Re-read the "Apply for hardware" tab for any changed requirements —
      overview-page requirements verified unchanged 2026-08-30 (started
      project link + BOM with UNO Q and Fusion + `.f3d` + application
      questions); the tab's own form questions still need a logged-in look
- [x] Update the snapshot doc with anything that differs — done 2026-08-30

## 2. Required application contents

Per the contest rules, the application is a link to a **started Hackster
project** plus form answers. The project page must contain:

| Requirement | Status | Owner |
|---|---|---|
| Beginning of the BOM, **including the UNO Q and Autodesk Fusion** | Ready to paste — §4 | [agent] prepared / [owner] pastes |
| **Fusion design/dataset (.f3d)** — first pass, not final | **IN PROGRESS — current handheld model establishes the larger display envelope** | [owner] |
| Project name, description, cover image | Draft ready — §3 | [owner] finalizes |
| Answers to all form questions | Drafts ready — §5 | [owner] edits and submits |
| Written in English | ✔ | — |

## 3. Project page copy **[agent draft — owner edits]**

**Name:** Platypus One

**Tagline:** An engineer standing beside you — a handheld that scans, measures,
inspects, and documents, where every capability is an app.

**Short description:**

> Platypus One is a handheld engineering multi-tool built on the Arduino UNO Q.
> Instead of a device that does one measurement, it runs PlatypusOS: an
> extensible operating environment where scanning, measuring, inspecting, and
> documenting are separate apps sharing one set of sensors. Point it at a part
> to capture geometry, measure it, check it against what it should be, and
> write a reviewed, Fusion-ready engineering record — in the field, offline, in one hand.

**Longer story beats** (expand these on the page; documentation is 20 of 100
points and the story is what carries them):

1. **The problem.** Engineering work away from the desk means juggling
   calipers, a phone camera, a notebook, and a laptop. The measurements end up
   in four places and get retyped.
2. **The idea.** One handheld where the *job* is what you select, not the
   sensor. "Reverse engineer this part" recruits camera, multizone depth, IMU, and
   geometry services together, then sends reviewed measurements into Fusion — see
   [engineering utilities](../roadmap/ENGINEERING_UTILITIES.md).
3. **Why the UNO Q specifically.** The dual-brain layout is the product, not a
   convenience: the QRB2210 Linux MPU runs vision, inference, and project
   storage while the STM32U585 handles deterministic sensor sampling and I/O.
   The split is enforced in software by a documented RPC protocol
   ([mcu-bridge.md](../protocols/mcu-bridge.md)), not left implicit.
4. **Left-hand grip, on purpose.** The device lives in the left hand so the
   dominant hand stays free to probe, mark, and adjust — the ergonomic logic of
   a flashlight or multimeter. See
   [industrial design](../hardware/INDUSTRIAL_DESIGN.md).
5. **Fusion's role.** Enclosure (hybrid CNC-aluminium barrel + polymer grip)
   and the carrier PCB schematic both live in Fusion. The contest workflow also
   exports reviewed engineering measurements into a parameterized Fusion design;
   Fusion is part of what the device produces, not only how its enclosure is made.
6. **Honest engineering.** The software builds and runs on a workstation with
   no hardware attached, so progress never blocked on parts. Decisions —
   including the selected larger DSI display direction — are recorded as ADRs
   in the repository.

**Cover image [owner]:** ⚠️ Use an original render or photo — your own Fusion
screenshot or a physical sketch. The concept sheets in `docs/media/` are
recorded in [INDUSTRIAL_DESIGN.md](../hardware/INDUSTRIAL_DESIGN.md) as
**AI-generated references**, and the contest requires original work. The
platypus mascot is fine if it is your own.

## 4. BOM excerpt for the project page **[agent]**

Paste this condensed table. This table is the authoritative **Hardware Request BOM**; it is deliberately curated from, but is not identical to, the master planning inventory in [docs/hardware/BOM.md](../hardware/BOM.md) (scope layering is governed in the private planning overlay). Both mandatory lines — the UNO Q and Autodesk Fusion — are present.

**Scope lock:** received research samples (catalogued in the private planning overlay) are not being requested from the contest and do not alter this application BOM.

| Item | Qty | Est. | Notes |
|---|---|---|---|
| **Arduino UNO Q (4G RAM)** | 1 | $59 | QRB2210 Linux MPU + STM32U585 real-time core |
| **Autodesk Fusion (+ Electronics)** | 1 | Free / contest licence | Enclosure, carrier PCB, schematics, and measurement-to-parameter handoff |
| **5" DSI capacitive-touch display + UNO Media Carrier** | 1 set | ~$55–60 | Larger display direction selected from the Fusion model; final sourcing/bench proof remains open |
| Rotary encoder, EC11-style push | 1 | $3 | Primary navigation control |
| Camera module, autofocus (USB UVC) | 1 | $25 | ShadowScan capture + documentation |
| **VL53L8CX-class 8×8 multizone ToF module** | 1 | ~$20 | 64-zone coarse depth, target/background separation, and camera-geometry cross-check |
| IMU, 9-DoF | 1 | $25 | Angle measurement, scan orientation |
| Colour sensor (TCS34725) | 1 | $8 | Colour measurement |
| High-CRI LED + driver | 2 | $2 | Controlled illumination for ShadowScan |
| LiPo battery + USB-C charge/boost | 1 set | $35 | ~2 h handheld operation |
| Speaker/piezo + amp | 1 | $6 | UI feedback |
| microSD 32 GB, USB-C supply, wiring, fasteners | — | $52 | |
| **Custom carrier PCB — PCBWay** | 1 | ~$60 | Designed in Fusion Electronics |
| **Enclosure — PCBWay** | 1 set | ~$120 | Hybrid CNC aluminium + printed grip |
| PlatypusOS (this repository) | — | — | C++20/CMake, own work |
| Arduino App Lab / UNO Q Linux image | — | Free | Board OS |

Say plainly on the page that part selection is still open where it is open —
"beginning of BOM" is what the rules ask for, and visible decision-making reads
better than false precision.

## 5. Form answer drafts **[agent draft — owner edits]**

The exact form questions are not recorded in the snapshot; confirm them
on the page. These cover what such applications typically ask.

**What are you building?**
> A handheld engineering multi-tool — Platypus One — running PlatypusOS, an
> extensible embedded OS where every capability (3D capture, measurement,
> inspection, documentation) is a separate app on shared hardware. It captures
> geometry and measurements from real parts in the field, preserves uncertainty,
> and exports reviewed parameters into an Autodesk Fusion design.

**Why the Arduino UNO Q?**
> The product needs both soft real-time perception and hard real-time sensing.
> The UNO Q's QRB2210 Linux MPU runs camera capture, vision, on-device
> inference, and storage; the STM32U585 owns deterministic sensor sampling,
> GPIO, PWM, and the encoder. My architecture already separates these across a
> documented bridge protocol, so the board's dual-brain design maps onto the
> software rather than being worked around.

**What is your experience / how far along are you?**
> The software platform is written and building host-side today: layered HAL,
> app framework, renderer, project storage, MCU bridge protocol and codec, unit
> tests, and a host simulator that runs the whole OS with no hardware attached.
> Hardware bring-up checklists, a BOM, and industrial-design direction are
> documented. What I need is the board itself.

**How will you use Autodesk Fusion?**
> Enclosure design and the carrier PCB, both in Fusion (Fusion Electronics for
> the schematic and layout), plus a contest workflow that maps reviewed,
> unit-aware Engineering Observations into named parameters in a Fusion design.
> The canonical observation remains CAD-neutral so later adapters can target
> other CAD programs without changing the sensor or evidence model.

**What will you do with the PCBWay credit?**
> One carrier PCB spin (sensor I²C hub, buttons, encoder, power routing) and the
> enclosure, with a planned respin gate in early November so a second spin can
> still land before the December freeze.

**Timeline?**
> Hardware from mid-September; bring-up against written test checklists through
> October; carrier PCB and enclosure ordered mid-October; hardware freeze late
> November; submission Dec 16–18, ahead of the Dec 20 deadline.

## 6. Fusion .f3d first pass **[owner — critical path]**

This is the only mandatory deliverable with no substitute, and the only one an
agent cannot produce. It does **not** need to be final — it needs to show the
plan.

Minimum for the application:

- [ ] UNO Q placed from the **official Arduino STEP model** (board outline
      68.58 × 53.34 mm per the mechanical drawing; do not derive a 3D
      clearance from the 2D outline — take it from the STEP)
- [ ] Selected **5-inch DSI touch-display envelope + UNO Media Carrier** represented
      in the assembly; use the current Fusion-model envelope until the final panel
      CAD/STEP and bench proof close the remaining sourcing risk
- [ ] Fat pistol grip body, left-hand orientation
- [ ] Camera at the barrel, forward-facing along the pointing axis
- [ ] Rotary encoder at the thumb (24 × 24 × 30 mm)
- [ ] Battery bay (100 × 60 × 8 mm flat cell envelope)
- [ ] USB-C at the grip base
- [ ] Plastic section reserved near the antenna — no metal over the radio
- [ ] Export `.f3d` and attach to the Hackster project

All envelope numbers come from [BOM.md](../hardware/BOM.md) rev B. Rough
massing is fine; unresolved parts should be visibly blocked out rather than
omitted.

## 7. Pre-submit verification **[owner]**

- [ ] Hackster project page is **published** (not draft) and the link opens in
      a private window
- [ ] `.f3d` is attached to the project and downloads correctly
- [ ] BOM on the page lists **both** the UNO Q and Autodesk Fusion
- [ ] Cover image is original work
- [ ] Every form question answered; all text in English
- [ ] Eligibility re-read: solo entry, 18+, eligible country
- [ ] Repository link is presentable if included — see the open licensing item
      in [STATUS.md](../../STATUS.md); the rules require citing licences for
      any third-party code, and this repository currently has no LICENSE file
- [ ] Submitted, with a screenshot of the confirmation saved
- [ ] Tick Phase 0 in [ACQUISITION_ROADMAP.md](../hardware/ACQUISITION_ROADMAP.md)

## 8. What accepting hardware commits you to

Accepting the hardware is a contract to build, document, and submit by
**Dec 20, 2026**, or return the hardware. The full-submission deliverables are
listed in the [contest snapshot](autodesk-au2027-contest-snapshot.md) — read
that list before accepting, not in December.
