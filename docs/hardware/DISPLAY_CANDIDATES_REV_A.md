# Rev A display candidates — 3.5–4.3" capacitive DSI (#41)

The 5" Waveshare panel on the bench is a development fixture: too large for
the handheld. This ranks off-the-shelf 3.5–4.3" capacitive MIPI-DSI panels for
the UNO Q + UNO Media Carrier (path A) or a future Platypus carrier (path B).
Researched 2026-10-04. Vendor figures are **vendor data**, not bench results.
Nothing is ordered.

## What decides compatibility

Matching the connector is not enough. A DSI panel needs a device-tree overlay
and a kernel panel driver with its exact controller and init sequence. On the
UNO Q that means Arduino's kernel 7.0 and its Media Carrier overlay slot (as
for the current panel), not Raspberry Pi's downstream overlays.

Established on our board (2026-10-04): the working 5" Waveshare **DSI LCD**
(800×480, Rev2.2) runs through the overlay in Arduino's 5-inch slot, whose
panel nodes are `raspberrypi,7inch-touchscreen-panel-regulator` (the ATTINY
controller at I2C 0x45) and `waveshare,4-3-inch-dsi` (the panel timing), over
the 15→22-pin MIPI-DSI-Cable-12cm. See [DSI_BRINGUP.md](DSI_BRINGUP.md).

## Candidates

| # | Panel | Size / res | Controller path | Connector | Outline (vendor) | Driver risk on UNO Q | Notes |
|---|---|---|---|---|---|---|---|
| **1** | **Waveshare 4.3inch DSI LCD** | 4.3", 800×480 IPS, 5-pt capacitive (Goodix) | Waveshare documents it on the Pi with `dtoverlay=vc4-kms-dsi-7inch`: the Pi 7" protocol (ATTINY regulator + bridge), i.e. the **same family our working 5" uses**, and the `waveshare,4-3-inch-dsi` timing already in our overlay | 15-pin FFC; 15→22 cable path already proven on this carrier | overall thickness 14.05 mm; outline from Waveshare 3D/2D files (to record) | **Low (expected, unproven):** likely the existing overlay unchanged | 1.2 W (vendor). Top of the size range; tests whether 4.3" fits the envelope at all |
| 2 | Waveshare **4-DSI-TOUCH-A** | 4.0", 480×800 IPS, GT911 | **ST7701S** over 2-lane DSI; needs an ST7701 panel descriptor with Waveshare's init sequence (mainline has an ST7701 driver family; this panel's sequence must be ported) | **22-pin** direct (MIPI-DSI and FFC-22 cables included) — same family as Arduino's supported TOUCH-A panels, but a different controller | 108.30 × 65.10 mm, active 86.40 × 51.84 mm | **Medium:** new panel entry + overlay, built like the current DKMS install | $28.99. Smallest proven-shape option; best product-size candidate if #1 is too big |
| 3 | Waveshare 4.3-DSI-TOUCH-A | 4.3", 480×800, GT911 | ST7701S, as #2 | 22-pin | 116.50 × 68.90 mm, active 93.60 × 56.16 mm | Medium, as #2 | Larger than #1's class with no driver advantage over #2 |
| 4 | Waveshare 4inch DSI LCD | 4.0", 480×800 IPS, optical bonding | Pi: `dtoverlay=vc4-kms-dsi-waveshare-panel,4_0_inch` — Raspberry Pi's downstream Waveshare panel driver, not the 7" path | 15-pin (22-pin cable for Pi 5) | from Waveshare drawing (to record) | **Medium–high:** port a downstream driver | Same size as #2 with a harder driver |
| 5 | Waveshare 3.5inch DSI LCD (E) | 3.5", 640×480 IPS, 5-pt | controller **not published** on the product page | 15-pin, opposite-sided cables included | in drawing (not extracted) | **Unknown** until the controller is identified | $32.99. Only DSI option at 3.5"; resolve the controller before considering |
| — | Bare 4" MIPI ST7701S + GT911 panels (generic suppliers) | 4.0", 480×800 | ST7701S | panel FPC: needs a custom adapter | supplier drawings | Medium driver, high supply/mechanical risk | Path B only (custom carrier) |
| ✗ | EastRising ER-TFT3.97-1 | 3.97", 480×800 | ST7701S over **SPI + RGB**, not DSI | — | — | Not DSI | Excluded |
| ✗ | Waveshare 4inch DSI LCD (C) | 4", 720×720 round | — | — | — | — | Round: excluded for this instrument UI |

Sources: [4.3inch DSI LCD wiki](https://www.waveshare.com/wiki/4.3inch_DSI_LCD),
[4inch DSI LCD wiki](https://www.waveshare.com/wiki/4inch_DSI_LCD),
[4-DSI-TOUCH-A](https://www.waveshare.com/4-dsi-touch-a.htm),
[3.5inch DSI LCD (E)](https://www.waveshare.com/3.5inch-dsi-lcd-e.htm),
[ER-TFT3.97-1](https://www.buydisplay.com/3-97-inch-480x800-ips-tft-lcd-capacitive-touch-screen),
[4" MIPI ST7701S panel](https://www.display-lcd.com/product_details/1433.html).

## Recommendation: buy and test next

**Waveshare 4.3inch DSI LCD (800×480).** It is the only small panel whose
documented control path is the one already running on this UNO Q, with the
same 15→22 cable. It is the cheapest way to separate *size* from *driver*:

1. Bench: same overlay, same cable orientation photos; record enumeration,
   backlight, touch and orientation against [DSI_BRINGUP.md](DSI_BRINGUP.md).
2. Fusion: drop its vendor 3D model into the Rev A-SM1 envelope study
   ([REV_A_SM1_MECHANICAL_REQUIREMENTS.md](REV_A_SM1_MECHANICAL_REQUIREMENTS.md)).

If 4.3" is too large in the mock-up, the next panel is the
**4-DSI-TOUCH-A (4.0", 108.3 × 65.1 mm)**, accepting an ST7701S driver port.

## Gates

- [ ] #1 enumerates on the UNO Q with backlight, touch and correct
      orientation (bench evidence, photos, overlay hash).
- [ ] Envelope: chosen panel fits the Rev A-SM1 mock-up with the required
      bezel/edge and service access.
- [ ] Supply: confirm the selected panel is a stocked catalogue part, and
      record the drawing revision used in Fusion.
- [ ] Driver path documented and reproducible from a clean board image.
