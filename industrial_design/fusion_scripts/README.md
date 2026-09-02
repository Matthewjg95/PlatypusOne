# Fusion 360 scripts

Scripts that run inside Autodesk Fusion 360 against the Platypus One
enclosure design. `.f3d` is a closed format, so geometry changes are
delivered as Fusion API scripts you run with the design open, rather than
as edited `.f3d` files.

## PlatypusOneBeefUp

Turns the flat display-enclosure into a handheld-instrument packaging study:

- Renames the existing enclosure bodies (`ENCLOSURE_TOP` / `ENCLOSURE_BOTTOM`,
  or `ENCLOSURE_SHELL` when the design has a single body).
- **GRIP** — raked pistol grip under the rear half, USB-C slot cut in the base.
- **SENSOR_HEAD** — forward head block with a camera opening, ToF opening and
  LED illumination window, plus `CAMERA_ENVELOPE`, `TOF_ENVELOPE`, `LED_ZONE`
  and a generic `SENSOR_CARRIER` inside.
- **UNO_Q** — official 68.58 × 53.34 mm board outline with an explicit
  clearance height (placeholder until the vendor STEP is dropped in).
- **DISPLAY_ENVELOPE**, **BATTERY** — envelopes from
  `industrial_design/bounding_boxes/BOM_v0_1.md`.
- **CONTROLS** — `ROTARY_ENCODER` at the thumb, `TRIGGER` at the index finger,
  `USB_C_PORT` in the grip base.

Every group is its own named component, so the browser tree reads like the
packaging plan. The script never cuts or modifies the pre-existing enclosure
bodies — it only renames them; all cuts happen on bodies it creates itself.
It finishes with a packaging report and calls out real conflicts (e.g. the
display + UNO Q stack vs. the current body thickness).

### Running it

1. Open the Platypus One enclosure design in Fusion 360.
2. Utilities tab → **ADD-INS** → *Scripts and Add-Ins* → green **+** next to
   "My Scripts" → select the `PlatypusOneBeefUp` folder from this repo.
3. Select the script → **Run**.

### Iterating

All dimensions live in the `CONFIG` dict at the top of the script (mm and
degrees). Edit values and run again: re-runs delete only the components the
script created and rebuild them, so iteration is safe. If the grip or sensor
head lands on the wrong face, flip `FLIP_FORWARD` / `FLIP_UP` / `FLIP_SIDE`
and re-run. Axes are auto-detected from the enclosure bounding box
(forward = longest axis, up = shortest).

### Known limitations

- Placeholder blocks and cylinders only — no organic grip sculpting; that is
  a manual pass (or a later script) once the layout is approved.
- Envelope bodies are reference geometry created via base features; they are
  positioned by the script, not driven by user parameters. Re-run to move
  them.
- The UNO Q clearance height defaults to 16 mm — replace with the official
  STEP model (`CAD/arduino_q.step`) before any production decisions, per the
  BOM note.
