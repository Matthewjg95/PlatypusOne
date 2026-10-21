# Enclosure mock-ups (hand-feel prints)

Printable STLs for judging size, grip and layout in the hand before any
enclosure is designed in Fusion (#41, #52). Plain Python, no CAD kernel: every
part is an extrusion, and each script writes a binary STL.

```sh
python tools/enclosure_mockup/display_dummies.py OUT_DIR   # 3.5" (H) and 4" display dummies
python tools/enclosure_mockup/device_tray.py OUT.stl       # open-top device tray
```

Print floor down, no supports. The meshes are overlapping closed shells, so
leave the slicer's "Union Overlapping Volumes" on (Cura's default).

## display_dummies.py

| Dummy | Source | Notes |
|---|---|---|
| Waveshare 3.5inch DSI LCD (H), SKU 33087 | VENDOR values in `docs/hardware/compact_display_mechanical.csv` (2D drawing + STEP) | PCB 1.6 mm, PCB centred on the lens, and the FFC block size are assumed |
| Waveshare 4-DSI-TOUCH-A, SKU 34354 | Waveshare dimension drawing, docs.waveshare.com/4-DSI-TOUCH-A#dimensions | Boss heights read from the side view; rounded glass corners and the FPC cut-out not modelled |

The active area is outlined by a groove rather than sunk: printed face down, a
sunk area would be a large bridge over air. True-size kiosk screen printouts
for both were made from `ui_preview` with the portrait layout from the
display-selection branch.

## device_tray.py

An open-top tray in the form factor of Matthew's 2026-10-17 render: left-hand
side grip, trigger and barrel camera pointing out of the grip's outer edge,
4" portrait display on the face. The real parts lie in it.

| Version | Change |
|---|---|
| v1 (2026-10-17) | Paddle: sensor head, display bay, compute bay in a line. Superseded: it lost the grip. |
| v2 (2026-10-17) | Side grip, barrel camera window, trigger hump, UNO Q stack above the display. |
| v3 (2026-10-20) | Camera **and** ToF side by side in the barrel behind one notch open to the rim; A068 battery stand-ins in the grip; charging-port slot. |
| v4 (2026-10-20) | Grip edge follows Matthew's sketched curve (waist, palm swell, sweep into the base); trigger moved in by its radius (tip level with the barrel face); battery stand-ins move under the display. |
| v5 (2026-10-20) | Trigger on the outside: the grip wall steps in under the barrel and runs behind the trigger 10 mm inside the barrel face, then rejoins the curve. The barrel overhangs; the trigger tip is level with the barrel face with a 4 mm finger gap. |

Every dimension's status is in the script's header. **Still estimated:** the
27 mm height, which is set by the UNO Q + Media Carrier stack (22 mm + 3 mm for
the display FFC bend). Measure it with calipers before the next version. The
sensor notch becomes proper windows once the camera/ToF bracket exists.

Each version was checked before printing: closed mesh (every directed edge
paired), and inside/outside probes at every wall, opening, rib and ledge.
