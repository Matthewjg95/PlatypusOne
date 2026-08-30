# UNO Q DSI panel candidates

**Research date:** 2026-08-25  
**Decision state:** DSI architecture remains selected; exact panel selection is conditional.

## Recommendation

Use the **Waveshare 5-DSI-TOUCH-A** as the first hardware proof-of-life
candidate, through the **Arduino UNO Media Carrier (ASX00083)**. It is the only
panel in the 4.3–5 inch target range for which the current UNO Q configuration
tool ships a named device-tree profile.

This changes the expected production geometry from “800×480-class landscape”
to **720×1280 native portrait, rendered as 1280×720 landscape if rotation is
supported by the compositor/application**. Do not freeze the enclosure around
this panel yet. Freeze only after the bench test below proves display, touch,
rotation, suspend/resume, and backlight control.

The DSI path is technically credible, but procurement is **not yet cleared**:

- Arduino's store listed the Media Carrier at €19.89 but “Coming Soon” and
  “Sold out” on the research date.
- The 5-inch panel is orderable from Waveshare, but its exact mechanical drawing
  still needs to be captured into the Fusion model before enclosure freeze.
- The current Arduino profile names this specific Waveshare family; generic
  Raspberry Pi DSI compatibility is not enough to establish UNO Q support.

If the Media Carrier cannot be obtained, or the proof-of-life test fails by the
existing **2026-09-30** gate, activate the ESP32-S3 RGB bridge fallback in
ADR-0002.

## What the UNO Q actually exposes

The UNO Q board does not expose a directly usable display FFC. Its raw
multimedia signals are on the underside **JMEDIA/JMISC high-speed board-to-board
connectors**. The supported breakout is the UNO Media Carrier.

The current carrier documentation establishes:

| Item | Verified value |
|---|---|
| Carrier | Arduino UNO Media Carrier, SKU **ASX00083** |
| Host connection | JMEDIA + JMISC underside connectors |
| Display receptacle | DSI0, **22 contacts, 0.5 mm pitch, four-lane-capable** Raspberry Pi “mini” FFC form |
| Logic | SoC control signals are in the **1.8 V** domain |
| Display interface | MIPI-DSI; touch/control uses the panel's I²C path |
| Carrier outline | 68.58 × 53.34 mm |
| Carrier power | Host rails; optional 7–24 V input for high-power configurations |
| Published receptacle MPN | **Not identified in Arduino's public datasheet/manual** |

The public Arduino material describes the DSI connector by physical/interface
class, but does not give its manufacturer part number or a text pin-by-pin table.
The safe design contract is therefore the documented Raspberry Pi-compatible
22-pin/0.5 mm interface and an included vendor cable—not an assumed receptacle
MPN. Obtain the ASX00083 schematic/CAD package and verify the footprint before
copying this connector onto any custom carrier.

The carrier is not merely a passive convenience. It supplies level translation,
power-control circuitry, and the supported physical path from the UNO Q's
high-speed connectors. A custom FFC adapter should not be the Rev A plan.

Sources:

- [UNO Media Carrier user manual](https://docs.arduino.cc/tutorials/uno-media-carrier/user-manual)
- [UNO Media Carrier datasheet source](https://github.com/arduino/docs-content/blob/main/content/hardware/02.uno/carriers/uno-media-carrier/datasheet/datasheet.md)
- [UNO Media Carrier store page](https://store.arduino.cc/products/uno-media-carrier)
- [Raspberry Pi 22-pin connector description](https://www.raspberrypi.com/documentation/computers/compute-module.html)

## Candidate comparison

Prices and availability are snapshots from manufacturer storefronts on
2026-08-25; re-check before ordering.

| Candidate | Mechanical/display facts | FFC / electronics | Current price | UNO Q Linux assessment |
|---|---|---|---:|---|
| **A. Waveshare 5-DSI-TOUCH-A** | 5 inch IPS, 720×1280 native portrait, optical bonding, five-point capacitive touch | 22-pin DSI family; panel profile is shipped by Arduino | **$34.95–36.99**, listed for purchase | **Recommended.** Exact named profile exists: `display=5-dsi-touch-a`. Still requires bench validation and landscape rotation validation. |
| **B. Waveshare 4.3-DSI-TOUCH-A** | 116.50 × 68.90 mm outline; 93.60 × 56.16 mm active area; 480×800 native portrait; 500 cd/m²; ST7701S + GT911; five-point touch | MIPI two-lane; kit includes reverse-contact 22-pin 0.5 mm FFCs; separate 5 V input documented | **$28.99**, Add to Cart | **Plausible, not supported.** Connector class and chips are credible, but no UNO Q profile exists. It needs a new panel/touch overlay and proof that the carrier's power/control routing matches. |
| **C. DFRobot DFR0550-V2** | 120.7 × 75.8 × 4.9 mm; 108.0 × 64.8 mm active area; 800×480; 550 cd/m²; optical bonding; five-point touch | Raspberry Pi DSI; 3.3 V, about 700 mA; supplied with 50 mm and Pi 5 300 mm ribbon cables | **$49.90**, orderable | **Low confidence.** Attractive mechanical fit and exact legacy resolution, but DFRobot documents Raspberry Pi compatibility only and Arduino ships no profile. Controller identity and UNO Q initialization sequence remain unverified. |

Candidate sources:

- [Waveshare 5-DSI-TOUCH-A product page](https://www.waveshare.com/5-dsi-touch-a.htm)
- [Waveshare 4/4.3-DSI-TOUCH-A product page](https://www.waveshare.com/4-dsi-touch-a.htm)
- [Waveshare DSI interface notes](https://docs.waveshare.com/Displays/LCD/DSI)
- [Waveshare 4.3-DSI-TOUCH-A technical page](https://docs.waveshare.com/4.3-DSI-TOUCH-A)
- [DFRobot DFR0550-V2 product page](https://www.dfrobot.com/product-2791.html)

### Why Candidate A wins

Arduino's current software registry is decisive. It explicitly provides
`5-dsi-touch-a`, `8-dsi-touch-a`, and `10-dsi-touch-a` display options for
the Media Carrier. Candidate A is the sole 4.3–5 inch option on that list.

Candidate B is the more compact industrial-design fit, and Candidate C preserves
the existing 800×480 assumption, but both would turn panel selection into kernel
integration work. Waveshare itself warns that DSI requires driver and
initialization support and that screens are not generally interchangeable.
Matching connector pitch and lane count therefore does not clear either panel.

The 5-inch portrait panel's resolution is not a renderer blocker because
PlatypusOS already treats display geometry as runtime data. It is, however, a
mechanical and UX change that must be tested before the housing is frozen.

## Debian/device-tree path

Do **not** hand-edit `config.txt` or manually install Raspberry Pi overlays on
the UNO Q. Arduino provides `arduino-linux-config`, which composes the
board-specific overlays into the boot DTB.

Expected workflow on a current UNO Q image:

```bash
sudo arduino-linux-config carrier list

sudo arduino-linux-config carrier enable media-carrier \
  display=5-dsi-touch-a

sudo arduino-linux-config carrier show media-carrier
sudo reboot
```

The current Arduino registry maps that option to:

```text
qrb2210-arduino-imola-carrier-media.dtbo
qrb2210-arduino-imola-carrier-media-panel-5in_touch_a-dsi.dtbo
```

It also marks the USB-C video/sound overlay incompatible with the DSI profile.
Internally, the tool runs `fdtoverlay` against
`/boot/efi/dtb/qcom/qrb2210-arduino-imola-base.dtb` and atomically replaces
`/boot/efi/dtb/qcom/qrb2210-arduino-imola.dtb`; the change takes effect after
reboot. Use the CLI/UI rather than reproducing that implementation manually.

Sources:

- [Arduino Linux Config CLI](https://github.com/arduino/arduino-linux-config)
- [UNO Q Media Carrier registry](https://github.com/arduino/arduino-linux-config/blob/main/internal/registry/registry.go)
- [Overlay application implementation](https://github.com/arduino/arduino-linux-config/blob/main/internal/dto/apply.go)

## Proof-of-life acceptance test

Order only the panel and carrier for the first pass. Do not commit the enclosure
or a custom carrier PCB until all checks pass.

1. Photograph labels, both FFC ends, contact orientation, and carrier/display
   connector orientation before assembly.
2. With power removed, install the Media Carrier and the panel using the supplied
   reverse-contact 22-pin cable.
3. Power from a supply with adequate headroom and enable
   `display=5-dsi-touch-a`.
4. After reboot, record:
   - `arduino-linux-config carrier show media-carrier`
   - `uname -a`
   - `cat /proc/device-tree/model`
   - `ls /sys/class/drm`
   - `dmesg` lines for DSI, DRM, panel, backlight, and touch
   - `libinput list-devices` or equivalent touch enumeration
5. Verify cold boot output, 30-minute display stability, all touch points,
   coordinate alignment, backlight adjustment, and three reboot cycles.
6. Verify application rendering in native portrait and intended landscape
   orientation. Record the physical active area and bezel with calipers.
7. Verify USB-C charging/data behavior while DSI is active; do not assume the
   incompatible USB-C video overlay implies all USB-C functions are unavailable.
8. Suspend/resume if the product image supports suspend, then repeat touch and
   backlight checks.

Pass criteria:

- No repeated DSI/DRM/panel errors in `dmesg`.
- Full-frame output with no clipping or persistent flicker.
- Touch is enumerated automatically and maps correctly after rotation.
- Backlight is controllable and survives reboot.
- The measured stack fits the intended barrel/display envelope.
- USB-C retains the charging/data behavior needed by the product.

## Order and fallback gates

| Gate | Result |
|---|---|
| At least two purchasable 4.3–5 inch candidates documented | **Pass** |
| A candidate has a shipped UNO Q software profile | **Pass: 5-DSI-TOUCH-A** |
| Media Carrier immediately purchasable from Arduino | **Fail on research date: Coming Soon / Sold out** |
| Exact production mechanical stack verified | **Pending hardware** |
| DSI + touch proof-of-life on UNO Q | **Pending hardware** |

Recommended order when available:

1. Arduino UNO Media Carrier ASX00083.
2. Waveshare 5-DSI-TOUCH-A, panel-only configuration with supplied FFCs.
3. No alternate panel in the first order; use the bench result to decide whether
   compactness justifies funding a custom 4.3-inch overlay.

Keep the 2026-09-30 fallback gate. Trigger fallback if the carrier is still
unobtainable, Candidate A cannot pass the acceptance test, or its rotated
mechanical envelope is incompatible with the industrial design.
