# Compact display bench acceptance (D0–D7)

For the leading candidate, the Waveshare 3.5inch DSI LCD (H). The same
procedure applies to the (E) with its `.panel` file. See the background in
[COMPACT_DISPLAY_SELECTION.md](COMPACT_DISPLAY_SELECTION.md).

- **Automation:** stages D1–D7 are automated where a machine can judge.
  The operator judges colours and touch position.
- **Evidence:** every run writes raw output plus `results.json` to
  `/var/lib/platypus-compact-display/evidence/<UTC>-<boot>/`.
- **Archiving:** copy that directory into the repo, or attach it to #41.
  A stage without preserved evidence did not happen.
- **PENDING** means the operator still owes an answer. It is never a pass.

## Before the panel arrives (nothing here changes the board)

1. On the UNO Q, build `display_probe` from this repo and put it on `PATH`.
   This is the same build as the 5" bring-up.
2. Clone upstream and check out the pinned commit:
   `git clone https://github.com/dcuartielles/uno_q_dsi_displays && git -C uno_q_dsi_displays checkout d633636527a213e884dae05468ffd355d12974f1`.
3. Install the tools: `sudo apt install device-tree-compiler i2c-tools v4l-utils`.
4. Record the 5" baseline once. This proves the fallback still works before
   anything changes:
   - run `display_probe`;
   - run `display_probe --input 10`;
   - save the output as `5in-baseline.txt`.

## Install (reversible)

```sh
sudo shutdown -h now                      # power OFF, swap the 5" for the 3.5" (D0 below)
# boot (5" overlay still installed; the 3.5" will be dark - expected)
sudo tools/compact_display/install_panel.sh ~/uno_q_dsi_displays
sudo reboot
```

`install_panel.sh` aborts before changing anything if any of these fail:

- the overlay does not compose against the board's DTB;
- the upstream checkout is not at the pinned commit;
- the carrier overlays are missing.

**Rollback:** `sudo tools/compact_display/restore_5in.sh ~/uno_q_dsi_displays && sudo reboot`.
Then repeat step 4 and compare against `5in-baseline.txt`.

## Stages

Run `sudo tools/compact_display/bench_accept.sh --boot-kind cold` after the
first boot. It runs every stage. `--stage Dn` re-runs one stage.

| Stage | Action | PASS (all of) | FAIL → first suspect |
|---|---|---|---|
| **D0 connect** | Power off. 22-pin end in the carrier DISPLAY connector exactly as the 5" photos ([DSI_BRINGUP.md](DSI_BRINGUP.md)). 15-pin end in the panel, contacts per the panel silkscreen. 5 V/3 A supply. Photo `D0-assembly.jpg` | Operator confirms; photo saved | — |
| **D1 identify** | Read the GT911 product ID (reg 0x8140) at the installed address and the other one. Probe 0x45 | `0x39 0x31 0x31` ("911") at the installed address, **and** nothing at 0x45 | No ID at either address → FFC orientation/seat, then 3V3 at the panel. ID only at the other address → reinstall (auto-detect picks it). 0x45 answers → a 5"/4.3" is still attached |
| **D2 boot** | Compare the slot overlay with the built one. Find our compatible in `/proc/device-tree`. Driver bound on the mipi-dsi device. Collect DSI/panel/goodix/CCI dmesg errors | Slot = build, node live, `panel-simple` bound | Not live → `arduino-linux-config carrier show`. Not bound → DKMS module did not load (`modinfo panel-simple \| grep platypus`) |
| **D3 enumerate** | `/sys/class/drm/card*-DSI-*` status and modes | `connected` with `480x800` | Connector absent → overlay graph. Mode wrong → panel-simple descriptor |
| **D4 render** | Stop the display manager; `display_probe --prefer dsi --pattern 25`; photo `D4-pattern.jpg` | Probe rc 0 at 480×800. **Operator:** red top-left, green top-right, blue bottom-left, white bottom-right, yellow frame on all 4 edges, no roll/flicker | Black with D3 PASS → panel not self-initialising on this host (main risk). Rolling/noise → link rate: retry with a lower `CLOCK_KHZ`. Swapped colours → format |
| **D5 touch** | Goodix in `/proc/bus/input/devices`; `display_probe --input 30`; tap TL, TR, BR, BL, centre | Device present **and** a `down` event inside each of the five 20 % zones | Device present, zero events → CCI 12-byte fault (Goodix fix not loaded). Points mirrored → add touchscreen-inverted/swapped props |
| **D6 repeat** | Each boot: rerun `--stage D6 --boot-kind cold\|warm`. Cold = supply unplugged ≥10 s; warm = `sudo reboot` | ≥5 cold **and** ≥5 warm boots since install, every one passing D1, D2, D3 and touch-device | Intermittent D1 → the CCI cold-boot outage seen upstream. Record which boot, and keep cycling to measure the rate |
| **D7 camera** | Pattern up; capture 90 frames (USB webcam auto, or `CAM_DEV=/dev/videoN`, or `CAM_CMD='…'` for the CSI module from #40); then 5-point touch again | Capture OK, no new DSI/CCI/goodix dmesg errors, DSI still connected, touch coverage still passes, operator saw no blanking | New CCI errors → polling vs camera bus contention: record, then compare with the touch driver unloaded |

**Overall PASS** = D0–D7 all PASS. The minimum to call the (H) *compatible*
is D1–D5 PASS. D6 and D7 decide whether it is *usable*.

## What a PASS does and does not mean

- **A PASS proves:** this panel unit, on this UNO Q image, with these files,
  shows correct pixels and takes touch, repeatably, alongside the camera.
- **A PASS does not prove:**
  - brightness or readability outdoors;
  - power headroom (POWER_TREE H2 measures that, with the panel lit);
  - mechanical fit;
  - long-run thermal behaviour;
  - behaviour after a kernel upgrade (DKMS rebuilds; rerun D2–D5 after one).

## After the run

1. Commit the evidence directory under `docs/hardware/evidence/compact-display-<date>/`.
2. In the selection doc, update the classes from BENCH REQUIRED to PROVEN or FAILED.
3. If D4 fails black on a working D3, stop. Restore the 5". Report on #41
   with `D2-dmesg-errors.txt`. The next candidate is the 4.3" (zero new software).
