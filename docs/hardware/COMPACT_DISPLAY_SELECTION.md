# Compact display selection (3.5–4.0") — UNO Q + UNO Media Carrier

Issue: [#41](https://github.com/Matthewjg95/PlatypusOne/issues/41). Written 2026-10-05.
**None of the candidates below has been connected to our UNO Q.** This page
says what is known, how it is known, and what the bench has to show.

| Class | Meaning here |
|---|---|
| **PROVEN** | Observed on *our* UNO Q + Media Carrier (bench notes / photos / logs in this repo) |
| **DOCUMENTED** | Written in a manufacturer document, kernel source, Arduino schematic, or a third party's recorded result on an UNO Q |
| **INFERRED** | Our reasoning from documented facts; plausible, not shown |
| **BENCH REQUIRED** | Settled only by a stage in [the bench procedure](COMPACT_DISPLAY_BENCH_ACCEPTANCE.md) |
| **BLOCKED / UNKNOWN** | No source available; says what would unblock it |

Raspberry Pi support counts as DOCUMENTED for *the panel's own behaviour*
(timings, touch chip, what the host must send). It never counts as UNO Q
support: the Pi uses the vc4 DSI host and its own overlays, and the UNO Q
uses the Qualcomm MSM DSI host plus Arduino's overlays.

## Decision

**Leading candidate: Waveshare 3.5inch DSI LCD (H), SKU 33087** — 480×800
portrait IPS, GT911 touch, 15-pin FFC, 3.3 V only.

- **Recommendation:** buy one as a bench evaluation unit. This is not a product freeze.
- **Fallback:** the 4.3inch DSI LCD. It carries no software risk, because the
  community has verified it on an UNO Q with the same `.panel` file as our proven 5".
  It is larger, and it is landscape-native.
- **Reasons for the (H)** are in [§6](#6-decision-matrix).

#41 says not to *order or freeze* until the Fusion envelope study is done.
This recommendation is for one evaluation unit. Driver compatibility is the
open question here, and only a panel on the bench can answer it. The aperture
stays unfrozen until D0–D7 pass and the mock-up agrees.

## 1. What the proven 5" gives us (PROVEN, [DSI_BRINGUP.md](DSI_BRINGUP.md))

| Reusable piece | Reused for the (H)? | Notes |
|---|---|---|
| Media Carrier DSI0 + `5-dsi-touch-a` slot + `arduino-linux-config` | **Yes, unchanged** | Our overlay goes in the same slot; the 5" overlay is backed up first |
| 15-pin 1.0 mm → 22-pin 0.5 mm FFC, orientation photographed | **Yes** | The (H) has the same Raspberry Pi 15-pin pinout (DOCUMENTED, Waveshare wiki) |
| panel-simple descriptor build (upstream `20-build-drivers.sh`, `25-install-dkms.sh`) | **Yes** | Our `.panel` file uses the upstream format. Generated patch checked against mainline panel-simple.c (§5) |
| **Single DSI lane on the MSM host** | **Yes** | The proven 5" already runs 1 lane at 27.777 MHz × 24 bpp = **667 Mbit/s** (upstream `waveshare-800x480.panel`) |
| ATTINY regulator at 0x45 + FT5x06 touch at 0x38 | **No** | The (H) has neither: no host-controlled power, reset or backlight |
| `DrmDisplay`, `display_probe`, kiosk | **Yes** | Touch detection is generic (`EV_ABS` + `BTN_TOUCH`, scaled to mode), so GT911 needs no code. No backlight device → `setBacklight` returns NotSupported, and nothing calls it |
| Kiosk layout (ADR-0001 geometry-driven) | **Yes, extended** | Portrait path added (§7). Landscape output byte-identical to main |

## 2. Candidates

### 2.1 Waveshare 3.5inch DSI LCD (H) — SKU 33087 — LEADING

| Item | Value | Class / source |
|---|---|---|
| Size / active area | 3.5", VA 45.36 × 75.80 mm; lens 52.56 × 88.87; PCB 56 × 86.5 | DOCUMENTED (2D drawing) |
| Resolution / orientation | 480 × 800, **portrait-native** | DOCUMENTED (wiki, `Waveshare_35DSI.dtbo` 35H) |
| Lanes / bit rate | FFC wires 2 lanes; Waveshare's overlay drives **1**. 33.6 MHz × 24 / 1 = **806 Mbit/s** | DOCUMENTED / DERIVED |
| Connector | 15-pin 1.0 mm. Pins 2/3 D1, 5/6 CLK, 8/9 D0, 11/12 SCL/SDA, 14/15 3V3 | DOCUMENTED (wiki pin table) |
| Power | 3.3 V, 180 mA, from the FFC; no 5 V | DOCUMENTED (Waveshare FAQ) |
| Touch | GT911, 5-point. Overlay lists 0x14 **and** 0x5d; wiki shows `10-0014 Goodix`. **No INT or RST on the FFC** | DOCUMENTED |
| Controller / init | **None from the host.** The Pi overlay is `vc4-kms-dsi-generic` with timings only: no regulator, reset GPIO, backlight or init sequence | DOCUMENTED (decompiled overlay → [reference JSON](../../tools/compact_display/reference/waveshare_35dsi_overrides.json)) |
| Backlight | Not adjustable; always on | DOCUMENTED |
| DT / DRM path on UNO Q | panel-simple descriptor (private compatible) + polled goodix + our overlay | INFERRED (§4); BENCH D2–D5 |
| Adapter | **Passive**: the proven 15→22 cable | DOCUMENTED (pin match) / BENCH D0–D1 |
| UNO Q software work | Done in this PR (`tools/compact_display/`); upstream Goodix CCI fix reused | — |
| Mechanics | Portrait. 56 mm PCB → 72 mm head with 8 mm edges, inside the 75–85 band. FFC exits the top short edge (STEP) | DOCUMENTED / DERIVED |

### 2.2 Waveshare 3.5inch DSI LCD (E) — SKU 33086

**Electrically and in software, a sibling of the (H):**

- same `Waveshare_35DSI.dtbo` (35E override);
- 1 lane at 24 MHz → 576 Mbit/s, *below* the proven 667;
- the same GT911 with no INT, and no controller;
- 640 × 480 landscape; VA 70.67 × 53.27; PCB 77 × 65.

`panels/waveshare-3in5-dsi-e.panel` is prepared and tested. Nothing on I2C
distinguishes it from the (H), so it carries no detect keys.

**Why it is not leading:**

- Landscape, 77.27 mm wide → 93.3 mm head. That is over the 75–85 band.
- Mounting it portrait needs software rotation, which `DrmDisplay` does not have.
- The 640×480 kiosk layout truncates the title in the side panel (§7).
- Its lower bit rate makes it the lower-*electrical*-risk 3.5". If the (H)
  fails at 806 Mbit/s but works with a slower clock, the (E) becomes interesting.

### 2.3 Waveshare 4-DSI-TOUCH-A — SKU 34354

**Panel (DOCUMENTED, Waveshare docs + `panel-waveshare-dsi-v2.c`):**

- 4.0", 480 × 800 portrait-native, ST7701S, GT911;
- **2 lanes**, video + HSE + LPM, non-continuous clock;
- a vendor **init sequence** sent by the host;
- **5 V 4.75–5.25 V, ≥180 mA input that the Media Carrier DSI connector does not carry**;
- software backlight through the panel's controller;
- 108.30 × 65.10 × 7.9 mm aluminium back, 8× M2.5.

**Software:** the compatible `waveshare,4.0-dsi-touch-a` exists **only in
`rpi-6.18.y`** (absent from 6.12–6.17). The upstream "derived panel" route
(as used for the community-verified 4-DSI-TOUCH-C) is the plausible path:
port the driver, plus an overlay derived from Arduino's 10.1" one
(INFERRED, not attempted).

**Risks:**

- the init sequence must survive the MSM host;
- the driver is new code on a 7.0 kernel;
- 5 V needs its own wire, or a Host Carrier provision;
- it is 108 mm tall.

**Status:** medium risk. Fallback if 3.5" proves too small in the mock-up.

### 2.4 Waveshare 2.8inch DSI LCD — SKU 22028 (lower bound)

- 480 × 640, VA 58.0 × 43.6, glass 71.3 × 52.9 (DOCUMENTED).
- The photographed PCB carries an **ICN6211** bridge (the 5" family) and has
  software backlight.
- Its exact host-side driver/overlay is **BLOCKED/UNKNOWN**: the wiki was not
  decoded this sprint.
- Reference only. It is smaller than the (H) in active area by 26 % and
  brings no electrical advantage.

### 2.5 Waveshare 4.3inch DSI LCD (fallback, not in the brief's list)

**DOCUMENTED (third-party UNO Q result):** upstream `waveshare-800x480.panel`
records the 4.3" verified on an UNO Q + Media Carrier, kernel 7.0.0, on
2026-09-08 (800×480, backlight, touch, no DSI errors). It is the same
definition as our proven 5", so for us it is **zero new software**.

**Mechanics:**

- PCB 106 × 68, depth 14.05 mm.
- Landscape-native, so portrait mounting needs rotation (84 mm head).
- Landscape mounting means a 122 mm wide head.

This was the #43 "next to buy" choice. It still separates size from driver,
but it is the larger size we are trying to leave.

### 2.6 Excluded

Waveshare 4-DSI-TOUCH-C (community-verified on UNO Q) and the other
round/square panels are excluded. Their glass is round or square, and their
outline works against the compact head (#41 comment 3).

## 3. Electrical compatibility

**Media Carrier DSI0** (Arduino ASX00083 datasheet §4.5; schematic sheet "DSI FLAT"):

- 22-pin 0.5 mm, 4 lanes + clock;
- I2C through a TCA9406 level shifter onto the SoC **CCI** controller;
- 3.3 V from the UNO Q `PWR_3P3V` buck (3.1–3.5 V; ABX00162 datasheet);
- **no GPIO lines**.

So a panel needing a host-driven reset, interrupt, enable or 5 V cannot get
it from this connector.

| | (H) | (E) | 4-DSI-TOUCH-A | 4.3" DSI LCD |
|---|---|---|---|---|
| Supply needed | 3V3 180 mA | 3V3 (not stated; INFERRED ≈ (H)) | **5 V ≥180 mA (extra wire)** | 3V3 (proven path) |
| Lanes used | 1 | 1 | 2 | 1 |
| Per-lane rate | 806 Mbit/s | 576 | ≈ (ST7701 mode) | 667 |
| Needs host GPIO | no | no | via its 0x45 controller (I2C) | via ATTINY 0x45 (I2C) |
| Touch IRQ | none → polling | none → polling | via controller | FT5x06 polled (proven) |
| Adapter | passive 15→22 | passive 15→22 | 22-pin direct + 5 V lead | passive 15→22 |

**INFERRED risks for the (H):**

1. **806 Mbit/s on one lane** is 21 % above the rate proven on this host (667).
   We hold no QRB2210 DSI PHY rate limit from Arduino or Qualcomm documents,
   and no UNO Q data above 667.
   *Mitigation:* if D3 passes and D4 rolls or tears, try a lower `CLOCK_KHZ` in
   the `.panel`. The panel's own tolerance is unknown.
2. **180 mA on `PWR_3P3V`**, a rail that also feeds the STM32, ANX7625 and Wi-Fi.
   Arduino gives no spare-capacity figure. The 5" (backlight also from 3V3) is
   already running from it, unmeasured.
   *Action:* POWER_TREE H2 measurement.
3. **Self-initialisation.** The Pi sends no init commands, so none are needed.
   But the vc4 host might send something implicit in its LP-to-HS handover
   that the MSM host does not. *Settled by:* D4.

## 4. DSI / touch / Linux DT

**DT path (INFERRED):**

```
&cci_i2c0  : goodix,gt911 @0x14, touchscreen-size 480x800, NO interrupts/reset
&mdss_dsi0 : vdda-supply=&pm4125_l5; panel@0 compatible "platypus,waveshare-3in5-dsi-h"
&mdss_dsi0_out : remote-endpoint=&panel_in; data-lanes=<0>
```

- These are the same three targets and the same supply as the upstream overlay
  running our 5" (PROVEN path). It drops the ATTINY regulator, backlight and
  edt-ft5x06 nodes, and adds the Goodix node.
- The upstream generator cannot express this. It drops touch whenever there is
  no panel controller, hence our
  [`gen_overlay.py`](../../tools/compact_display/gen_overlay.py).
- **Panel driver:** mainline `panel-simple` has no generic DSI "timings from DT"
  panel, so the descriptor is compiled in. This is the same mechanism as the
  proven 5". The compatible is private, so nothing built in can win the bind
  race (the lesson of upstream's 4-DSI-TOUCH-C notes).
- **Touch:** mainline `goodix.c` falls back to `input_setup_polling` at 17 ms
  when `client->irq <= 0` (DOCUMENTED, source read).
  - Without a reset GPIO the driver cannot choose the address. It uses whatever
    the GT911 latched at power-up. Hence `install_panel.sh` reads the product ID
    at both 0x14 and 0x5d before generating the overlay.
  - Goodix reads exceed the CCI controller's 12-byte limit. The upstream fix
    (`17-install-goodix-fix.sh`, measured by its author on an UNO Q) is reused.
- **Polling cost:** every 17 ms on the bus that also serves the carrier's
  devices. Camera CCI coexistence is D7. Whether a CSI camera shares this CCI bus
  is UNKNOWN; bench required.
- **Rotation:** none needed for the (H) in a portrait head. Both 3.5" panels
  need `touchscreen-size` only; no swap/invert properties are expected.
  D5 corner taps check this.

## 5. Software prepared (reversible) — `tools/compact_display/`

| File | What it does | Verified here |
|---|---|---|
| `panels/waveshare-3in5-dsi-h.panel`, `…-e.panel` | Upstream-format panel definitions + GOODIX/detect keys, every value sourced in the header | Unit tests vs the decoded vendor overlay; 60.0 Hz; bit rates |
| `gen_overlay.py` | Controller-less overlay generator | Overlays compile with `dtc -@`; fixups for `cci_i2c0`, `mdss_dsi0`, `mdss_dsi0_out` |
| `install_panel.sh` | Pre-flight → backup → GT911 address probe → generate → `fdtoverlay` test-compose against the board's DTB (aborts before changing anything) → upstream driver build + Goodix fix → slot install + enable | `--dry-run` in CI; shellcheck. **Never run on a board** |
| `restore_5in.sh` | Re-runs the exact proven 5" install; `--offline` restores backed-up overlay + module binaries | shellcheck. **Never run on a board** |
| `bench_accept.sh` | Stages D0–D7 → evidence dir + `results.json` | shellcheck. **Never run on a board** |

The upstream `gen-panel-patch.py` (pinned d633636) was run against mainline
`panel-simple.c` with the (H) `.panel`. It inserts a 480×800 descriptor:
htotal 640, vtotal 875, 33.6 MHz → 60.0 Hz, 1 lane, RGB888, `MODE_VIDEO`.
This shows the patch applies textually. It was not compiled against the
UNO Q kernel headers.

The proven 5" config is not modified. `install_panel.sh` copies the 5" slot
overlay, carrier settings and module hashes and binaries to
`/var/lib/platypus-compact-display/backup-<UTC>/` before it touches anything.
The 5" and the compact panel cannot both be installed: they share one slot,
and panel-simple carries one generated descriptor. Switching is reinstall + reboot.

## 6. Decision matrix

Scores run 1–5, where 5 is best. Weights reflect #41: size first, then integration risk.

| Criterion (weight) | 3.5" (H) | 3.5" (E) | 4-DSI-TOUCH-A | 4.3" DSI LCD | 2.8" |
|---|---|---|---|---|---|
| Fits SM1 portrait head band 75–85 mm (×3) | **5** (72 mm, spare) | 2 (93 mm landscape) | 4 (81 mm) | 3 (84 mm, rotation) | 5 |
| Usable UI area (×2) | 3 (45×76 mm) | 3 (71×53) | 4 (52×86) | 5 | 1 |
| Electrical simplicity on DSI0 (×2) | **5** 3V3 only, passive | **5** | 2 (+5 V wire) | **5** (proven) | 3 (unknown) |
| UNO Q software risk (×3) | 3 (generic panel, no init; new overlay) | 3 (same; lower bit rate) | 2 (init seq + 6.18 driver port) | **5** (community-verified; our 5" def) | 1 (unknown) |
| Native orientation matches portrait head (×1) | **5** | 1 | **5** | 1 | 4 |
| Brightness / backlight control (×1) | 2 (270 cd/m², fixed) | 2 (fixed) | 4 (software) | 4 (software) | 4 |
| Mechanical data quality (×1) | **5** (2D + STEP) | 4 (2D) | 4 | 3 | 3 |
| **Weighted total (/65)** | **52** | 38 | 43 | 52 | 37 |

**The (H) and the 4.3" tie.** The tie breaks on the brief:

- the target is 3.5–4.0", with 3.5 preferred;
- the 4.3" is the size #41 was opened to leave;
- the 4.3" needs rotation work in a portrait head.

So the 4.3" is the **fallback**, and its value is precisely that it needs no
new driver work.

The 4-DSI-TOUCH-A ranks third. It is credible, but its 5 V lead and driver
port make it a deliberate second step only if 3.5" proves too small in the
mock-up.

## 7. Kiosk portrait finding (software, host-verified)

`scout_kiosk --offscreen-size WxH` renders the real loop at any geometry.
`computeLayout` now stacks preview over panel when height > width.
Landscape takes the original path unchanged: the 800×480 output is
byte-identical to main.

- **480×800:** usable without change (CI smoke added).
- **640×480 (the (E)):** the side panel truncates "ENGINEERING SCOUT".
  This is recorded, not fixed. It belongs to UI work, which is out of scope.

## 8. Risks and what retires them

| Risk | Class | Retired by |
|---|---|---|
| (H) does not light without Pi-specific init | INFERRED | D3 (mode) + D4 (pixels) |
| 806 Mbit/s single lane unstable | INFERRED | D4 photo + D6 repeat; fallback lower clock / (E) |
| GT911 address varies by unit / boot | DOCUMENTED (both listed) | D1 + D6 (address logged each boot) |
| Polled touch misses taps or loads CCI | INFERRED | D5 corner coverage, D7 |
| 3V3 rail headroom with 180 mA panel | INFERRED | POWER_TREE H2 (measure with panel + camera) |
| Always-on backlight (no dimming → power, glare) | DOCUMENTED | Accept for Rev A, or the 4-DSI-TOUCH-A later |
| Cold-boot CCI outage seen upstream on ATTINY panels | DOCUMENTED (upstream) | D6 cold boots. No ATTINY here, so only touch can be affected |
| FFC route length/bend in the head | UNKNOWN | Fusion route → cable length (MECHANICAL inputs) |

## 9. Mechanical inputs for Fusion

[`compact_display_mechanical.csv`](compact_display_mechanical.csv) is in Fusion
Parameter I/O format, with the same naming and `disp_edge_min` as PR #43's
`rev_a_sm1_parameters.csv`. It fills the 3.5" rows #43 left UNRESOLVED:

- outline;
- active area and its offset (the (H) VA sits 3.63 mm from the top and 9.44 mm from the bottom);
- depth;
- hole pattern (49 × 58, the Raspberry Pi pattern);
- FFC edge and position.

**Effect on the SM1 proportions:**

- The (H) is narrower than every other candidate: 72 mm with 8 mm edges.
- The head band (75–85) is kept. The spare 3–13 mm is left to the mock-up
  (grip, edge margin); it is not spent.
- 88.9 mm of glass leaves 36–56 mm of the 125–145 mm height for the sensor
  flange and controls.
- The FFC leaves at the top edge, toward the sensor flange. Fusion must route
  it past the camera/ToF datum bracket without loading it.

The enclosure is not redesigned here.

## 10. Bench queue

See [COMPACT_DISPLAY_BENCH_ACCEPTANCE.md](COMPACT_DISPLAY_BENCH_ACCEPTANCE.md).
