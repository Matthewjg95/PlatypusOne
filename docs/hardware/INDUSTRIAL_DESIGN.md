# Platypus One — Industrial Design Notes

Source material in [docs/media/](../media/): three concept sheets, a dimension
sketch, Skannerz enclosure inspiration, and the platypus mascot.
Status 2026-08-06: concept phase; **left-handed operation is a design driver**.

## Concepts on the table

| Concept | Sheet | Form | Hold |
|---|---|---|---|
| A. Pistol-grip | IMG_8050 / Prototype sketch 1 | 6.5"×3.8" (165×97 mm), screen + rear grip with trigger, top camera w/ LED ring, rotary encoder, speaker | One-handed, trigger under index |
| B. Landscape tablet | IMG_8051 | 6.8" (172 mm) wide, 6–7" LCD, overmold grips both sides, encoder + button cluster right, kickstand, ¼"-20 tripod mount | Two-handed |
| C. Vertical handheld | IMG_8052 | Phone-like, camera top, side encoder, trigger under index, kickstand, tripod mount | One-handed |
| Vibe ref | Skannerz | Organic asymmetric shell, chunky rubberized texture, bold accent buttons | — |

Common DNA across all three: front camera with LED ring, push rotary encoder,
scan trigger under the index finger, stereo speakers + mic, USB-C + microSD,
kickstand, tripod mount, expansion port for clip-on modules (thermal camera,
structured-light scanner, macro lens, battery). Keep all of these — they map
1:1 onto the HAL (ICamera, buttons/encoder via IMcuBridge, IAudioOutput,
IStorage, ISensorHub for modules).

## Left-handed operation — implications

Concepts A and C as drawn are right-hand layouts (grip/controls fall under the
right thumb). For left-hand primary use:

1. **Enclosure mirror (concepts A/C):** grip and trigger move so the *left*
   index finger rests on the trigger; rotary encoder and button cluster land
   on the *left* edge for the left thumb; screen shifts right in front view.
   In Fusion this is a mirror body + re-routed part placements, not a redesign
   — decide handedness **before** the carrier PCB is frozen, since button/
   encoder positions live on that board.
2. **Symmetric + software (recommended):** design the shell near-symmetric
   (concept B is already close), put the encoder on top-center or both edges,
   and let software own handedness: a Settings toggle rotates the UI 180° and
   remaps buttons. One SKU serves everyone — this matters for the contest,
   since the winning design gets manufactured (~750 units) for a general
   audience that is ~90% right-handed.
3. Camera/ToF must sit on the device centerline (or top edge) so scan sweep
   ergonomics are identical in either hand.
4. Kickstand + tripod mount are handedness-neutral — keep.

**Software follow-up:** add a `handedness` setting (Settings app) exposed via
AppContext so apps can mirror layouts; renderer gains a 180° present rotation.
Cheap now, painful later. (Tracked in ROADMAP M2.)

## Locked direction (2026-08-06, per Matthew)

The concept sheets were AI-generated references only. Actual design intent:

- **Left-hand grip by design rationale**: the device lives in the left hand so
  the dominant right hand stays free to work (probe, mark, adjust). Same
  ergonomic logic as a flashlight or multimeter — this *benefits* right-handed
  users and is the story for the contest writeup.
- **Fat pistol grip**, largest screen that fits the front face.
- **Rotary encoder at the thumb** — for a left-hand grip that's the upper-left
  edge/face region from the user's viewpoint.
- **Camera at the "gun barrel"** — forward-facing along the natural pointing
  axis, so aiming the scanner is instinctive.
- **Sensors + expansion GPIO on the screen face or around the sides** —
  keeps modules visible/reachable while gripped.
- **Charging port at the bottom** (grip base) — cable hangs naturally, dock
  potential later.
- Physical sketching in progress; Fusion model to follow from those sketches.

## Main body in drawn metal — feasibility

Wish: deep-drawn metal main body. Reality by build stage:

| Stage | Verdict | Notes |
|---|---|---|
| Prototype (1–2 units, this contest) | **Deep drawing: no.** Tooling (punch + die) runs $5k–50k and only pays off at volume; PCBWay doesn't offer it as a prototype service | Use **CNC-machined aluminum** for the metal parts instead — PCBWay does this well; same look/feel, no tooling |
| AU 2027 production (~750 units) | **Borderline.** 750 is low for deep-draw tooling amortization; **die-cast aluminum or CNC** is the more likely production path. Note it as a "design for manufacture" consideration in the writeup — judges like that thinking | A drawn shallow shell (screen bezel/barrel sleeve) is more drawable than a full body |
| Geometry limits | A fat pistol grip with undercuts is a **poor deep-draw shape** (drawing wants shallow, cup-like, uniform-wall parts) | Grip should be polymer regardless — see hybrid below |

**Engineering cautions with any metal body:**

1. **RF**: the UNO Q's Wi-Fi/BT antenna cannot live inside a metal shell.
   Reserve a plastic window/end-cap near the antenna, or plan an external
   antenna pigtail. This is the #1 metal-body mistake — decide the antenna
   window location in the first Fusion pass.
2. **Thermal (the upside)**: the QRB2210 runs warm under Linux load; a metal
   barrel section doubles as a passive heatsink — thermally couple the SoC to
   it with a gap pad.
3. **Weight**: CNC aluminum walls at 1.5–2 mm keep the handheld balanced;
   solid-billet designs get heavy fast.
4. **Cost**: a CNC aluminum barrel/faceplate will likely consume most of the
   $300 PCBWay credit alone. Budget split to sanity-check at quote time:
   metal barrel ≈ $150–250, printed grip ≈ $40–80.

**Recommended hybrid** (best of both, and very much the power-tool idiom):

- **Metal (CNC alu, bead-blasted/anodized)**: the "barrel" — camera/sensor
  head and screen surround. Premium feel where fingers don't wrap, heatsink
  where the SoC lives.
- **Polymer (printed now, molded at volume)**: the fat grip, with rubber
  overmold texture per the Skannerz reference. Warmer in hand, RF-friendly,
  houses battery + USB-C at the base.
- Accent color on encoder + trigger (mascot bill orange).

Skannerz takeaway for the shell language: chunky radii, rubber overmold
texture, one saturated accent color on the encoder/trigger (mascot platypus
bill orange would be on-brand).

## Open questions

- [ ] Final display: options + recommendation consolidated in
      [DISPLAY_COMPARISON.md](DISPLAY_COMPARISON.md) (2026-08-24). Decide
      before the Fusion model firms up — it drives aperture, depth, battery.
- [ ] Trigger: mechanical microswitch (via MCU GPIO) vs capacitive.
- [ ] Expansion port connector selection (24-pin high-density is drawn).
- [ ] Kickstand: magnetic folding (drawn) vs simple flip — PCBWay printability.
