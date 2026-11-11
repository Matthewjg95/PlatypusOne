# Four-inch display gate checklist

Companion to [DISPLAY_4IN_GATE.md](DISPLAY_4IN_GATE.md). Updated 2026-10-28. **No physical display, electrical, mechanical or CSI coexistence gate has passed.**

## Non-physical software gate

- [x] Identify the 4-DSI-TOUCH-A controller's upstream compatible, mode, init sequence, lane count and source provenance (Git blob c6cc003a35337971eef670587b200928ca1c1814).
- [x] Add a fail-closed source preflight and compile-only (no installation) harness with synthetic tests. This does **not** mean the module compiled against the UNO Q kernel.
- [ ] Run the compile-only probe on the **UNO Q's matching kernel headers** and archive source blob, kernel release, patch.log, build.log and result.json.
- [ ] Validate generated overlay/DTB offline against the actual board's base DTB and Media Carrier overlay without installing it. Check GPIO polarity, regulator/power assumptions, and private compatible.
- [ ] Preserve an executable recovery path for the known working five-inch display before any panel configuration changes.

## Physical gates — remain OPEN

- [ ] Verify panel/Media Carrier connector pinout, 5 V supply polarity/current capability and shared ground.
- [ ] Confirm screen pixels and touch axes on the actual panel.
- [ ] Repeat cold/warm boots and capture simultaneous CSI camera operation.
- [ ] Confirm Fusion envelope, cable routing, left-hand grip and optical alignment.
- [ ] Record a final procurement/fit decision, distinguishing 4.0 A, 3.5 H and older 4.3 ICN6211 alternatives.

A source audit, synthetic test, CI success or successful module compile is **not** proof that a panel lights, responds to touch, or fits.

## 2026-11-04 gate disposition — executable evidence requirements

**Current repository inspection:** PR #57 is still open. The branch is 20 commits ahead of main (not an isolated display-only diff), and main's latest indexed commit is camera characterization #46 (61f18294). Therefore do **not** merge this branch wholesale to close a display gate. Keep the 5-inch fixture operational and review the stacked ancestry before any merge/rebase.

**No target kernel compilation was observed in this run.** The existing preflight has synthetic source/patcher/make tests; their PASS is not a kernel compatibility result. The runner lacks a verified UNO Q kernel build tree, the board's actual `uname -r`, and a matching DTB. The next subgate is blocked on those inputs, not on a camera capture or new PCB.

### Compile-only artifact acceptance (non-physical)

1. Record `uname -r`, `uname -a`, OS image/build ID, `readlink -f /lib/modules/$(uname -r)/build`, and whether `/lib/modules/$(uname -r)/build/Makefile` exists. If headers are missing or mismatch the running kernel, record BLOCKED; do not claim compile failure or substitute generic desktop headers.
2. Use the reviewed upstream source commit `97b5af2dde50c0acace6d406208dec3fc8dfe888`; verify `git hash-object panel-waveshare-dsi-v2.c` equals `c6cc003a35337971eef670587b200928ca1c1814`. Record the exact patcher repository revision and SHA-256.
3. Run `preflight_4in.py --driver ... --compile --patcher ... --out ...` as an unprivileged user in an empty output directory. Preserve `patch.log`, `build.log`, `result.json`, resulting source, `Makefile`, and the exit code. Never run `insmod`, `modprobe`, `dtc -@ -I dtb -O dtb` against live boot assets, or alter `/boot` during this gate.
4. Accept **COMPILE_PASS** only with a real matching UNO Q kernel build tree, genuine `make` and successful module output. A mock `make` or host-only syntax check remains **SYNTHETIC**. If the build fails, preserve diagnostics and classify as missing prerequisites versus source/API incompatibility.
5. Separately obtain the actual UNO Q base DTB and Media Carrier display overlay as immutable copies for offline decompile/compose checks. Inspect DSI endpoint/lanes, GPIO and regulator phandles, touch I²C ownership, and 5 V wiring assumptions; do not infer them from Raspberry Pi overlays. **Overlay validation remains OPEN even after a compile pass.**

### Physical acceptance still requires operator evidence

The actual 4-inch A panel, verified 5 V power polarity and capacity, screen and touch response, repeatable boots, concurrent CSI capture, and Fusion envelope measurements. Do not promote the 4-inch candidate from target to proven until these are archived. No BENCH/MECH/PCB gate is closed by this document.


## 2026-11-11 convergence audit — CI verified; target compatibility still blocked

**GitHub evidence inspected 2026-11-11:** draft [PR #57](https://github.com/Matthewjg95/PlatypusOne/pull/57) is open and mergeable, but its head `747ff198c5d884350b2e7f12332cd75f9a47fcdf` remains a stacked branch rather than a clean display-only change. The commit's GitHub Actions **Host CI** and **Display tools** runs both completed successfully. These are host/source/synthetic checks, **not** a module compilation against matching UNO Q kernel headers or DTB validation. No board kernel release, matching build tree, base DTB or Media Carrier overlay was found in the inspected gate records. Do not mark COMPILE_PASS or release panel/enclosure.

**New integration context:** draft [#58](https://github.com/Matthewjg95/PlatypusOne/pull/58) (trigger + BMI270 + VL53L8CX firmware, based on #49) and [#59](https://github.com/Matthewjg95/PlatypusOne/pull/59) (Linux trigger/IMU capture, based on #56) are **not on main**. Their simulated I²C/protocol tests cannot establish shared Qwiic bus current, electrical compatibility, camera/DSI coexistence or physical capture. Their presence also increases the cost of merging a broad stacked display branch without review. Keep DSI panel 5 V sourcing separate from the 3.3 V Qwiic sensor budget; neither power path is validated by host CI.

**Release rule:** advance the next display subgate only with immutable evidence bundle containing (a) board kernel/image identification and matching build-tree provenance, (b) real compile log/result, and (c) separately, offline DTB/overlay composition against the actual UNO Q/Media Carrier assets. If prerequisites are absent, record BLOCKED_MISSING_BOARD_ARTIFACTS rather than COMPATIBLE or INCOMPATIBLE. Maintain working five-inch fallback. No physical BENCH/MECH gates or PCB release were changed by this audit.
