# Dream Lab entry — maker.io post draft, form answer, and shot list

Working draft for the **Arduino UNO Q Dream Lab Challenge** entry. Written
2026-09-30 against the official rules and the state of the bench that day.

## The rules that shape this post

From DigiKey's challenge page and official rules (read 2026-09-30):

| | |
|---|---|
| Deadline | **11:59 p.m. CT, September 30, 2026** |
| How to enter | (1) build with the UNO Q, (2) publish a **maker.io project post** with **#UNOQDreamLab**, (3) submit the official form with the post link |
| Form asks | name, email, phone, project link, and *"If you won the Dream Lab, what would you do with it?"* |
| Eligibility | US resident, 18+, registered **MyDigiKey** account, one entry per person |
| Judging | five criteria at **20% each**: creativity/originality, technical execution, effective use of the UNO Q, **quality of documentation**, potential impact/innovation. Tie-break: creativity, then execution |
| Rights | "All entries … become the property of DigiKey"; broad licence to reproduce, modify and publicise; entrant warrants the entry is original and non-infringing |

What that means for the post:

- **Documentation is a full fifth of the score.** Show the loop working on
  the real board, with real photos, real numbers and the failures.
- **"Effective use of the UNO Q"** rewards showing *both* halves of the board
  (Linux MPU and STM32 MCU). Say clearly what each half does today.
- **Keep the post scoped to Engineering Scout.** PlatypusOne Core's industrial
  design and Rev A hardware are the Autodesk AU 2027 entry. Leave them out of
  this post rather than handing them to a second sponsor under a "property of"
  clause. The code is already public under Apache-2.0, so linking the repo
  gives nothing new away.
- **Credit third-party work** (originality warranty): the panel overlay and
  drivers from `dcuartielles/uno_q_dsi_displays`, Arduino's Zephyr core and
  LED-matrix library, and AI coding assistance.

---

## Post draft

> Placeholders are `[PHOTO: …]` and `[VIDEO]`. Numbers marked **(update)**
> should be replaced with today's bench data if a validation run happens
> before posting. Step 5 (sessions) is verified on the host only: confirm it on
> the board, or cut it to one line under "What's next", before posting.

### Engineering Scout: a UNO Q instrument that measures a part and says what it doesn't know

**#UNOQDreamLab**

Put an unknown screw on a card beside a printed 20 mm square and tap
CAPTURE. Engineering Scout measures it, names what it probably is, and saves
an engineering record. The record separates what the camera **observed**, what
was **derived** from that, what is only **inferred**, and what is still
**unresolved**, and it names the next view that would answer the unresolved
part. If the scene can't be measured honestly, Scout refuses and tells you
what to change instead of guessing.

[PHOTO: the whole bench — UNO Q on the Media Carrier, 5" panel showing a result card, webcam over the calibration card]

[VIDEO: under one minute — part down, tap, card, second capture in worse light, refusal + guidance, finish]

#### 1. Why

Every workbench has a jar of unidentified fasteners. Identifying one means
calipers, a thread gauge and a chart. Worse, most "AI measurement" demos report
a confident number whether or not the photo supports it. I wanted the
opposite: an instrument that is useful in ordinary, imperfect light and honest
about its uncertainty. It should be the first function of a real engineering
tool, not an object-detection demo.

#### 2. Hardware

| Part | Role |
|---|---|
| Arduino UNO Q | Qualcomm QRB2210 Linux side runs camera, vision, inference, UI and storage; STM32U585 MCU side runs Zephyr |
| Arduino UNO Media Carrier (ASX00083) | 22-pin MIPI-DSI "DISPLAY" connector for the panel |
| Waveshare 5inch DSI LCD, 800×480, capacitive touch (Rev2.2) | the instrument's screen and CAPTURE / FINISH buttons |
| Waveshare MIPI-DSI-Cable-12cm (15→22 pin) | panel ↔ carrier |
| USB webcam (640×480 YUYV) + USB-C hub | the camera, on the UNO Q's USB-C port |
| Printed calibration card with a 20 mm reference square | in-frame scale anchor |

[PHOTO: cable orientation, both ends — docs/media/hardware/dsi-cable-carrier-end.jpg and dsi-cable-panel-end.jpg]

#### 3. How it works (from the operator's side)

1. The panel shows a live camera preview and a guidance panel.
2. Tap **CAPTURE**. The UNO Q grabs a frame, finds the 20 mm square
   (the scale) and the part, measures the part, and classifies it.
3. The **evidence card** shows the headline measurement (e.g. `40.2 × 8.0 mm`,
   `bolt_or_screw ~M8`) and four colour-coded sections: OBSERVED (pixel
   facts), DERIVED (mm/px, length, width), INFERRED (class and nominal size with
   confidence bars), UNRESOLVED (thread pitch, bolt vs screw), plus NEXT (the
   view that would resolve them).
4. **Refusal is a feature.** No square in view, a part that runs off the frame
   edge, or a speck too small to be a part gives a plain instruction ("No part
   found beside the square. Keep it fully in frame.") and no number. The
   attempt is still saved as evidence.
5. **It never forces a part into a class.** A bolt or screw needs a head
   (one end of the outline clearly wider than the shank); a nut or washer
   needs one centred hole in a round or hex outline. When I put a PCB down,
   Scout measured it (41.8 × 25.7 mm) and recorded *"not a recognized
   fastener"*, with no thread or size questions. My test screw is a
   1/4-20 UNC. From above, a 6.35 mm shank can't be told from an M6 (6.00 mm)
   at ±0.5 mm, so Scout says **"1/4-20 UNC or M6"** and names the thread pitch
   (1.27 vs 1.0 mm, side view) as the thing that decides.
6. **Sessions.** Captures of one object are grouped. Scout asks for more
   ("shift or rotate the part") until three captures agree within 0.5 mm,
   then FINISH writes a CAD handoff bundle: the silhouette as **DXF (mm)** for
   Fusion 360 / KiCad, as JSON for a mesh-to-CAD tool, the source image and a
   summary.

[PHOTO: evidence card on the panel for a real screw]
[PHOTO: session summary screen with the exported outline]

#### 4. Software architecture

Written in C++20, with no OpenCV and no cloud:

- **Camera:** V4L2, YUYV at 640×480.
- **Display:** KMS/DRM directly — a dumb framebuffer on the DSI panel, touch
  and buttons from evdev. A `platypus-mode` switch toggles between the kiosk
  and the stock desktop, because only one of them can own the screen.
- **Vision:** illumination flattening (a robust surface fit that removes
  lighting gradients), Otsu thresholding, connected components, reference
  square detection, rejection of blobs clipped by the frame edge and of
  specks under 10 mm², and a marching-squares outline tracer.
- **Inference:** a fastener classifier over the measured geometry with
  nominal-size matching (M-series), reporting confidence rather than a bare
  label.
- **Evidence contract:** every capture is an append-only
  *Engineering Observation* JSON record with provenance on every claim, saved
  next to its source image. Inference can never masquerade as measurement.
- **Tested off the board:** the same code builds on Windows (MSVC) and Linux
  (GCC) in CI; a synthetic ground-truth battery and a renderer let the whole UI
  be reviewed without hardware.

**UNO Q split today:** the Linux side does perception, inference, UI and
storage. The STM32 runs the status LED matrix (an animated platypus). A
Linux↔MCU bridge protocol is written and host-tested but not yet on the board.
Next, the MCU takes over the deterministic work: capture trigger, controlled
illumination, and a turntable for multi-view capture.

#### 5. Results — honest numbers

**Synthetic ground-truth battery (24 cases):** 24/24 behave as specified;
classification 20/20, nominal size 15/15 (the true size among the named
candidates); length error 0.32 mm mean / 0.66 mm max; width 0.25 / 0.49 mm;
all 4 intentional failure cases refuse for the right reason, and the 3
out-of-library parts come out "unknown".

**First light on real hardware (29 Sep, late night, poor lighting):** the
pipeline ran end to end on the UNO Q with the real webcam. The reference
square was found correctly. A 1/4-20 UNC screw (calipers: 44.45 mm long,
9.48 mm head, 6.3 mm thread) measured **49.6 × 12.1 mm** — a large
over-read, from the part's shadow under a single low light and, it turned
out, an out-of-focus lens (below). Refusals worked as designed (for
example, a frame with the square but no part → "No part found").
**(update)** with the lighting matrix below if run before posting.

**Tonight's bench, against calipers (30 Sep, focus fixed, camera tilted).**
A hand-held instrument will never look straight down, so I measured how much
the tilt costs and how much the reference square can win back. "Today" is the
kiosk's scale from the square's area; "corrected" uses the square's four
corners to undo the tilt of the table plane (offline analysis of the same
frames; the board's analyzer gets it next).

*1/4-20 UNC socket-head screw*

| | Today | Corrected from the square's corners | Calipers / standard |
|---|---|---|---|
| Head diameter | 10.17 / 10.45 mm | **9.73 / 9.62 mm** | 9.48 mm |
| Shank (threaded section) | 6.72 / 7.30 mm | **6.08 / 6.20 mm** | 6.35 mm major |
| Thread pitch | 1.452 / 1.447 mm | **1.284 / 1.230 mm** | 1.270 mm (20 TPI) |
| Length | 52.9 / 55.9 mm | 49.3 / 50.4 mm | 44.45 mm |

The square's sides measured 145–164 px in the same frame: the camera was
tilted roughly 20°. The thread pitch is readable from a part lying flat, with
no side view, once the lens is focused: the edge of the thread gives a
period 20–30× above the noise.

*Adafruit OV5640 camera board (an out-of-library part, correctly reported as
"not a recognized fastener")*

| Scan | Width today | Width corrected | Calipers |
|---|---|---|---|
| 0045 | 24.84 mm (+8.8%) | 23.31 mm (+2.1%) | 22.83 mm |
| 0046 | 24.88 mm (+9.0%) | 24.45 mm (+7.1%) | 22.83 mm |
| 0047 | 24.43 mm (+7.0%) | 23.69 mm (+3.8%) | 22.83 mm |

(Length is left out: the pin headers stick out past the 35.90 mm board.)

What's left after the correction is height, not tilt. A screw lying down
sits a few millimetres above the paper and a PCB's components stand proud,
and anything nearer the camera looks bigger. That is the next correction
(below).

#### 6. What went wrong (and what I learned)

- **The display stayed dark across two bench sessions.** The panel never answered on I²C. The
  Waveshare 15→22 cable is *opposite-sided*: the gold fingers are on different
  faces at each end, so "same face up at both ends" is wrong. I found it with a
  continuity beeper because I don't own a multimeter. (Photos of the correct
  orientation are above.)
- **Enabling DSI turns off DisplayPort over USB-C** and left the port in
  device mode, so no webcam. The kiosk service forces the port to host mode
  and hands it back on exit.
- **My captures were soft, and I blamed the webcam.** Sharpness scores (the
  variance of a Laplacian filter) were 3–6, against hundreds for a sharp frame. Blur
  widens every edge, and a silhouette measured from blurred edges reads big:
  the shank came out 7.2 mm instead of 6.35. The fix was physical: the
  camera has a manual focus ring.
- **The classifier assumed.** The first version called anything long and thin
  a screw, and asked thread questions about a PCB. Now every class needs
  positive evidence, and the validation battery includes a headless rod, a
  PCB-like plate and a square plate with a hole, all of which must come out
  "unknown". The battery also caught an analyzer bug: a large square plate
  could be mistaken for the 20 mm reference and silently rescale everything.
- **Shadows are the enemy of silhouettes.** Illumination flattening fixed a
  lighting-gradient failure; shadows still inflate measurements. Controlled
  light from the MCU is the next fix.

#### 7. What's next

- **Tilt on screen.** Scout reads the reference square's distortion and tells
  the operator when the camera is tilted, before anyone trusts a number.
- **Undo the tilt automatically** from the square's corners (proven above on
  the bench frames), then **correct for height**: the corner correction also
  gives the camera's pose, and a part's own width tells how far its outline
  sits above the paper.
- **Depth sensing in the handheld.** The PlatypusOne handheld adds a
  multizone time-of-flight sensor: the table plane's tilt and distance come
  from depth, and the square becomes a cross-check rather than the only
  source of scale.

- STM32 capture trigger and ring light; bench validation across lighting
  conditions.
- Side-view capture for thread pitch and bolt vs screw.
- More object profiles (PCBs, flat brackets, turned parts), and multi-view
  capture toward 3D.

#### 8. Code and credits

- Code (Apache-2.0): https://github.com/Matthewjg95/PlatypusOne
- DSI panel overlay and drivers:
  [dcuartielles/uno_q_dsi_displays](https://github.com/dcuartielles/uno_q_dsi_displays)
- Arduino UNO Q Zephyr core and LED-matrix library
- Built with AI coding assistance (Claude Code)

---

## Form answer — "If you won the Dream Lab, what would you do with it?"

> Draft, about 100 words; edit to your voice.

I'd turn Engineering Scout from a bench prototype into a validated
instrument. Right now I debug hardware with a continuity beeper — that's how I
found the DSI cable fault that kept this display dark across two bench
sessions. A scope and
meter would let me bring up the STM32-driven capture trigger and ring light
properly, measure my own power budget, and verify the carrier I'm designing
for the handheld version. A proper bench also means real calibration and
validation data: measurements checked against gauges across lighting
conditions, published so other makers can trust the numbers — not just see a
demo.

---

## Shot list (about 45 minutes with the board)

1. **Hero photo:** board, carrier, panel on, webcam, calibration card. Daylight if
   possible.
2. **Evidence card on glass** for a real screw (photograph the panel).
3. **Lighting matrix:** 3–4 captures each in even light, uneven overhead,
   shadow, and rotated. Note calipers truth, measured value and any refusal.
   That fills §5's table.
4. **One refusal on purpose** (cover the square) → guidance on screen.
5. **One session:** three captures → FINISH → summary screen; open
   `outline.dxf` in Fusion 360 for a screenshot.
6. **Video under 60 s:** part down → tap → card → worse light → refusal →
   fix → finish.
7. Pull the records: `scp -r arduino@192.168.1.32:PlatypusOne/observations .`
   (and `sessions`).
