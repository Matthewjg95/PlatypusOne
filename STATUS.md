# Project Status

Rolling snapshot of where Platypus One actually is. Updated as work lands —
if this file disagrees with the code, the code wins and this file is stale.

**Last updated: 2026-08-30**

## Right now

| | |
|---|---|
| **Deadline in focus** | Hackster hardware application, **Sep 7, 2026** (extended from Aug 23) — target submit **Sep 4** |
| **Critical path** | The Fusion `.f3d` first pass. Owner-only work; nothing else in the application is blocked |
| **Hardware in hand** | M5Stack Tab5 (prototype display fixture). No UNO Q, no sensors |
| **Software state** | Builds and runs host-side; no hardware drivers exercised. Engineering Observation contract v0.1 merged (PR #10, review-corrected): `services/observation` with JSON round-trip + contract validation, GCC-12-safe |
| **Display** | **Conditional:** MIPI-DSI via UNO Media Carrier + Waveshare 5-DSI-TOUCH-A; hardware proof pending — [candidate research](docs/hardware/DSI_PANEL_CANDIDATES.md). Geometry remains runtime-dynamic per [ADR-0001](docs/adr/0001-dynamic-linked-prototype-display.md) |

## Milestones

- **M0 Foundation** — complete. HAL, app framework, renderer, host simulator,
  unit tests, architecture docs.
- **M1 Board bring-up** — partial. MCU bridge protocol and Linux driver written
  (untested against hardware); STM32 firmware, camera, sensors, cross-compile,
  and CI outstanding.
- **M2 Runtime maturity** — well underway. Win32 simulator window, runtime
  geometry, bounded input event queue (PR #5), and dirty-region present
  (PR #6) landed; settings store outstanding.
- **M3 Flagship apps** — not started. Six app directories hold READMEs only.
  Engineering Scout Q (issue #9) groundwork underway: evidence contract merged;
  camera→artifact→record capture slice is the next chunk in flight.
- **M4 Platform opening** — not started.

Detail and tags: [docs/ROADMAP.md](docs/ROADMAP.md).

## Decisions on record

| ADR | Decision |
|---|---|
| [0001](docs/adr/0001-dynamic-linked-prototype-display.md) | Display target stays dynamic; prototype on a linked external display; geometry discovered at runtime |
| [0002](docs/adr/0002-dsi-production-display.md) | Production display = 4.3–5″ MIPI-DSI 800×480-class capacitive, native Linux drive; bridge demoted to fallback (gate: Sep 30) |

## Open decisions blocking work

| Decision | Blocks | Notes |
|---|---|---|
| Production panel proof | Enclosure freeze, final battery sizing | **Conditional recommendation:** Waveshare 5-DSI-TOUCH-A through UNO Media Carrier. Carrier was not orderable and hardware was not tested as of 2026-08-25; see [candidate research](docs/hardware/DSI_PANEL_CANDIDATES.md) |
| ToF: VL53L1X vs VL53L8CX | Carrier PCB freeze | ⚖ in [BOM](docs/hardware/BOM.md) |
| IMU: BNO055 vs BMI270 | Carrier PCB freeze, first sensor driver | BNO055 recommended on schedule grounds |
| Camera: UVC OV5640-class vs 13 MP module | Camera driver path | UVC recommended (V4L2 is the simple path) |
| Encoder in the HAL vs MCU-bridge topic | `display/linked` input mapping | Presentation link already reserves the message |
| Espressif carrier identity (Korvo vs LCD-EV-Board SUB3) | Second prototype display option | Panel specs agree; board identity does not |

## Known gaps

- **Repository is private** (checked 2026-08-30, renamed
  `Matthewjg95/PlatypusOne`) — LICENSE landed (Apache-2.0, PR #3); it still
  needs to go **public** before the repo link goes on the Hackster page.
- **`platypus_board_unoq` compiles on UNIX only** and is linked by no
  executable, so the UNO Q board path is unbuilt on the Windows dev machine and
  unreachable from the composition root.
- **Test coverage** is registry, MCU framing, and renderer only. `ProjectStore`,
  `SerialMcuBridge`, `SettingsApp`, and `LauncherApp` are untested.
- **`third_party/` is empty** — Catch2 vendoring still undecided.
- **No `.f3d` in the repository.** Required contest deliverable.

## Verification status

The agent working this repository has **no reachable build toolchain** —
`cmake` requires interactive approval its sandbox cannot obtain. Code changes
made under those conditions are committed with a `NOT COMPILED` note in the
commit message. Current such changes:

| Commit | Change | Status |
|---|---|---|
| `feat(sim): make simulated display geometry a runtime choice` | Sim display geometry injection, `--geometry`, launcher row scaling, new geometry test | ✅ **Verified 2026-08-24** on the MSVC machine: clean build, all tests pass, `--geometry 800x480` sim window smoke-tested |
| main @ merge of PRs #1–#4 (incl. observation contract v0.1, PR #10) | Full host target | ✅ **Verified 2026-08-30** on the new dev machine (VS 2022 Build Tools, MSVC 14.44, CMake 4.4.3): fresh configure, clean Release build, 100% tests pass |
| main @ merge of PRs #5–#7 | Event queue, dirty regions, link framing | ✅ **Verified 2026-08-30** locally: clean Release build, all 7 test suites pass **with live asserts** — note Release configs previously compiled `assert` out (`NDEBUG`), so earlier Release test runs verified nothing; fixed in `fix(tests): undefine NDEBUG` |

Verification protocol: the MSVC machine session runs build+tests on pull and
updates this table; TARS marks new code changes `NOT COMPILED` until then.
As of 2026-08-30 the MSVC machine is the new Windows 11 dev box, and **Host CI
(PR #3) now builds and tests every push/PR on windows-latest** — CI green is
the primary signal; this table records local hardware-adjacent verification.

## Next up

0. **[owner/procurement] DSI proof-of-life** — research complete:
   [DSI_PANEL_CANDIDATES.md](docs/hardware/DSI_PANEL_CANDIDATES.md).
   Acquire the UNO Media Carrier + Waveshare 5-DSI-TOUCH-A when the carrier is
   available, then run the documented display/touch/rotation test. Keep the
   **Sep 30** ESP32-S3 fallback gate.
1. **[owner]** Fusion `.f3d` first pass →
   [application checklist](docs/contest/HARDWARE_APPLICATION_CHECKLIST.md) §6.
2. ~~Confirm on the live contest page whether the Sep 18 recipients date moved
   with the extension.~~ **Done 2026-08-30: it moved — recipients announced
   Sep 25, 2026 5:00 PM PDT** (Sep 7 close and Dec 20 submission confirmed
   unchanged). ACQUISITION_ROADMAP's bring-up window loses a week.
3. ~~Run the host build and tests to clear the verification table.~~ **Done
   2026-08-30** — see Verification status; local MSVC toolchain restored on the
   new machine.
4. ~~`infra/format` + CI + LICENSE~~ **Done 2026-08-30:** PRs #3 (Apache-2.0 +
   MSVC host CI) and #4 (clang-format/ruff policy + CI format checks) merged;
   PRs #1 (requirements baseline, status Proposed) and #2 (DSI panel research)
   merged alongside.
5. ~~`appfw/event-queue`, then `renderer/dirty-rects`~~ **Done 2026-08-30:**
   PRs #5, #6, and #7 (presentation-link framing codec, shared with future
   firmware) reviewed and merged in order; `display/linked` is now unblocked
   for wiring into the composition root.
6. STM32 firmware implementing [mcu-bridge v1](docs/protocols/mcu-bridge.md):
   Ping/Pong plus GPIO loopback is the smallest proof of the dual-brain
   architecture, and [checklist §2](docs/hardware/TEST_CHECKLISTS.md) is already
   written for it.
