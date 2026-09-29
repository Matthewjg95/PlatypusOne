# DSI display bring-up — UNO Q + UNO Media Carrier + Waveshare 5" 800×480

The concrete plan for linking the three boards on the bench, written
2026-09-28 from the hardware actually in hand. It supersedes the bring-up
procedure in [DSI_PANEL_CANDIDATES.md](DSI_PANEL_CANDIDATES.md), which assumed a
different panel and a newer board image than the ones on the bench.

## As-built hardware (from photographs, 2026-09-28)

| Board | Identification | DSI connector |
|---|---|---|
| Arduino UNO Q | SBC, kernel 6.16.7 image (launch era, Oct 2025) | high-speed connectors to the carrier |
| Arduino UNO Media Carrier | SKU ASX00083, "DISPLAY" connector | **22-pin, 0.5 mm pitch** |
| Waveshare 5inch DSI LCD | 800×480, capacitive touch, Rev2.2, ICN6211 DSI→RGB bridge on-panel | **15-pin, 1.0 mm pitch** |

**This is not the panel the candidates document recommended.** Candidate A
there is the Waveshare *5-DSI-TOUCH-A* (720×1280, stock Arduino profile). The
panel on the bench is the older Raspberry Pi–style *5inch DSI LCD* (800×480).
Arduino ships no profile for it, so `arduino-linux-config ... display=5-dsi-touch-a`
would configure the wrong panel.

It is, however, a verified configuration elsewhere:
[`dcuartielles/uno_q_dsi_displays`](https://github.com/dcuartielles/uno_q_dsi_displays)
carries `panels/waveshare-800x480.panel`, whose header records it *"Verified
working on Arduino UNO Q + UNO Media Carrier, kernel 7.0.0 … Waveshare 5inch DSI
LCD, 15-pin FPC via a 15->22 pin adapter"* — this exact panel and topology.

Electrical summary from that definition: 1 DSI lane, RGB888, 27.777 MHz pixel
clock; an ATTINY-style regulator/backlight controller at I²C `0x45` (REG_ID
reads `0xc3` or `0xde`); FT5x06 touch at `0x38`, X and Y inverted, held in reset
by the ATTINY until the driver releases it.

## The three gaps, and what closes each

| # | Gap | Closed by | Blocks |
|---|---|---|---|
| 1 | **No cable joins them.** Cables in hand are 15↔15 (fits the panel) and 22↔22 (fits the carrier). | A **15-pin 1.0 mm ↔ 22-pin 0.5 mm DSI FFC** — the Raspberry Pi 5 display cable (200 mm is plenty). | Everything physical |
| 2 | **The board image predates the carrier.** `ls /boot/efi/dtb/qcom/ \| grep carrier-media` returns nothing; `arduino-linux-config` 0.2.0 installs but fails (`qrb2210-arduino-imola-base.dtb` missing). | The `arduino-unoq` meta-package, which pulls in `linux-image-7.0.0` and the carrier overlays. Installing it directly fails on `alsa-ucm-conf`; `scripts/10-update-os.sh` from the project above resolves that by pinning Arduino's ALSA build. **No reflash.** | Panel driver |
| 3 | **PlatypusOS cannot draw on a local Linux display.** The only `IDisplay` implementations are `LinkedDisplay` (tiles to the Tab5) and the Win32 simulator. | **`DrmDisplay` (PR #27)** — raw KMS ioctls, one XRGB8888 dumb buffer, RGB565 converted on present; `tools/display_probe` to test it. Everything short of `SETCRTC` is verified on the board. | The demo UI |

Gap 3 does not wait for gaps 1 and 2. DRM is DRM: the backend can be written
and proven tonight against **HDMI through the UGREEN hub** (the `DP-1`
connector, DisplayPort alt-mode over USB-C), and the same binary drives
`DSI-1` once the panel is up.

## Constraints to respect

- **Enabling DSI disables DisplayPort over USB-C** — one display controller
  serves both. HDMI through the hub is the fallback demo display, so it stays
  available until the panel is proven; do not enable DSI before the cable is
  fitted and the panel has been detected.
- **Power: a dedicated 5 V / 3 A supply.** The project reports that USB-port
  power (0.5–0.9 A) produces intermittent I²C failures, dead backlights and
  "silent display failures that appear software-related". The PD charger
  feeding the hub must deliver 3 A at 5 V after the hub's own draw.
- **lightdm + Xorg own the display.** The stock image starts a desktop on
  `:0`, which holds DRM master; `DrmDisplay::open()` then fails with `Busy`
  and says so. Stop it for the demo: `sudo systemctl stop lightdm` (or
  `disable` it for a kiosk boot).
- **Device node numbers move between boots.** The webcam was `/dev/video2`
  one boot and `/dev/video0` the next. Select by capability or name —
  `bench_session.sh` and `DrmDisplay` do — never by a remembered number.
- **No RTC battery on the UNO Q.** Until NTP syncs, apt fails signature checks
  with "Not live until …". It looks like a broken mirror; it is the clock.
- **Never write to the panel controller's REG_PORTC by hand** — per the
  project's root-cause notes, that wedges the I²C bus. Reads are safe.
- Fit and remove FFCs with the board **powered off**; contacts toward the
  connector's contacts, lock bars fully closed.

## Plan

`[owner]` = physical work or anything needing the board's sudo password.
`[agent]` = done over SSH.

### Sep 28 night — done without the owner

Everything that needed no password and no cable:

- Baseline on kernel 6.16.7: SSH, native build, `platypus_tests`, camera
  capture (after fixing a double-open regression in the `--watch` branch —
  it, not the camera, was returning `Busy`).
- The panel photographed dark (webcam luma 2.1): expected, nothing joins
  the 22-pin carrier cable to the 15-pin panel cable.
- `DrmDisplay` + `display_probe` (PR #27): builds warning-free on the board;
  the probe enumerates the real MSM resources and refuses cleanly with
  `no connected display (DP-1 disconnected)`. A one-off check confirmed MSM
  allocates, ADDFBs (XRGB8888, depth 24) and maps an 800×480 dumb buffer
  (pitch 3200) without DRM master.
- **Not done, deliberately:** the OS update. Passwordless sudo on this image
  covers `apt-get install --only-upgrade` only, which cannot install the new
  kernel, and running it alone would move `alsa-ucm-conf` to exactly the
  backports build that blocks `arduino-unoq`. It waits for the owner.

### Next session — before the cable arrives

1. `[owner]` Order the 15↔22 DSI cable if not already done; confirm the PD
   brick's 5 V rating is ≥ 3 A.
2. `[owner]` Plug a monitor or TV into the hub's HDMI port, then:

   ```powershell
   ssh -t arduino@192.168.1.32 "sudo systemctl stop lightdm"
   ```
3. `[agent]` `display_probe --prefer dp --pattern 20` — first pixels from
   PlatypusOS code on real glass, and the first `SETCRTC`. Check the four
   quadrants read red | green / blue | white and all four yellow edges show.
4. `[owner]` OS update (20–40 min):

   ```powershell
   ssh -t arduino@192.168.1.32 "git clone https://github.com/dcuartielles/uno_q_dsi_displays.git && cd uno_q_dsi_displays && sudo ./scripts/10-update-os.sh && sudo reboot"
   ```
5. `[agent]` On `7.0.0`: carrier overlays present, camera found, rebuild,
   tests, validation battery, and step 3 again — HDMI stays usable until DSI
   is enabled.

### Sep 29 — cable arrives

8. `[owner]` Power off. 22-pin end into the carrier's DISPLAY connector, 15-pin
   end into the panel. Power on.
9. `[agent]` `sudo ./scripts/detect-panel.sh --list` must report
   `waveshare-800x480` via the ATTINY at `0x45`. If nothing answers at `0x45`,
   stop: cable orientation or power, not software.
10. `[owner]` `sudo ./install.sh panels/waveshare-800x480.panel && sudo reboot`
    (builds the patched panel/touch modules under DKMS, installs the overlay
    and the cold-boot recovery service).
11. `[agent]` `40-verify.sh`, `test-display.sh`, `test-touch.sh`;
    `/sys/class/drm` shows a connected DSI connector at 800×480. Record in
    [TEST_CHECKLISTS.md](TEST_CHECKLISTS.md) §3.
12. `[agent]` `display_probe --pattern` on `DSI-1` (the probe prefers DSI by
    default), then `display_probe --input` for touch through the FT5x06.
13. `[agent]` The demo loop on the panel: live camera preview, capture on
    touch or button, the Scout result card.

### Sep 30 — submit

14. Film the loop, write it up, submit. If steps 8–11 slipped, the identical
    loop runs on the HDMI monitor proven in the next-session step 3.

## Rollback

- Panel install: `sudo ./uninstall.sh && sudo reboot` restores Arduino's
  original overlay (`*.dtbo.arduino-orig`) and stock modules (`*.ko.distrib`).
- Kernel: the 6.16.7 image package stays installed alongside 7.0.0.
- If the board stops booting or leaves the network: USB-C straight to the PC
  (no hub) brings back ADB — `C:\Users\Public\adb\adb.exe`.
