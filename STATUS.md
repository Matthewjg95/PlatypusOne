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
| **Display** | **Decided:** MIPI-DSI production panel — [ADR-0002](docs/adr/0002-dsi-production-display.md); geometry still runtime-dynamic per [ADR-0001](docs/adr/0001-dynamic-linked-prototype-display.md) |

## Milestones

- **M0 Foundation** — complete. HAL, app framework, renderer, host simulator,
  unit tests, architecture docs.
- **M1 Board bring-up** — partial. MCU bridge protocol and Linux driver written
  (untested against hardware); STM32 firmware, camera, sensors, cross-compile,
  and CI outstanding.
- **M2 Runtime maturity** — started. Win32 simulator window and runtime
  geometry landed; dirty rects, event queue, settings store outstanding.
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
| ~~Production panel~~ | — | **Decided 2026-08-24** ([ADR-0002](docs/adr/0002-dsi-production-display.md)): DSI. Open sub-item: which exact panel — see TARS task below |
| ToF: VL53L1X vs VL53L8CX | Carrier PCB freeze | ⚖ in [BOM](docs/hardware/BOM.md) |
| IMU: BNO055 vs BMI270 | Carrier PCB freeze, first sensor driver | BNO055 recommended on schedule grounds |
| Camera: UVC OV5640-class vs 13 MP module | Camera driver path | UVC recommended (V4L2 is the simple path) |
| Encoder in the HAL vs MCU-bridge topic | `display/linked` input mapping | Presentation link already reserves the message |
| Espressif carrier identity (Korvo vs LCD-EV-Board SUB3) | Second prototype display option | Panel specs agree; board identity does not |

## Known gaps

- **No LICENSE file.** The contest requires citing licences for third-party
  code. The repository is currently **private** (checked 2026-08-30, renamed
  `Matthewjg95/PlatypusOne`) — it needs a LICENSE and then to go public before
  the repo link goes on the Hackster page.
- **No CI.** Nothing catches a broken host build between commits.
- **No `.clang-format` / ruff config**, though the coding standards reference
  both (ROADMAP `infra/format`).
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

Verification protocol: the MSVC machine session runs build+tests on pull and
updates this table; TARS marks new code changes `NOT COMPILED` until then.

## Next up

0. **[TARS] DSI panel research** — blocking follow-up from
   [ADR-0002](docs/adr/0002-dsi-production-display.md): UNO Q DSI connector
   part/pinout, ≥2 purchasable 4.3–5″ 800×480 DSI capacitive panels with
   Linux driver plausibility (price, lead time, FPC, active area), and the
   Debian image's device-tree overlay mechanism. Deliverable:
   `docs/hardware/DSI_PANEL_CANDIDATES.md`. Gate: fall back to the ESP32-S3
   bridge if nothing viable by **Sep 30**.
1. **[owner]** Fusion `.f3d` first pass →
   [application checklist](docs/contest/HARDWARE_APPLICATION_CHECKLIST.md) §6.
2. ~~Confirm on the live contest page whether the Sep 18 recipients date moved
   with the extension.~~ **Done 2026-08-30: it moved — recipients announced
   Sep 25, 2026 5:00 PM PDT** (Sep 7 close and Dec 20 submission confirmed
   unchanged). ACQUISITION_ROADMAP's bring-up window loses a week.
3. **[owner]** Run the host build and tests to clear the verification table.
4. `infra/format` + CI + LICENSE — cheap, and they protect the host-sim path
   while attention moves to hardware.
5. `appfw/event-queue`, then `renderer/dirty-rects` — in that order, since
   `display/linked` needs both before it can be wired into the composition root.
6. STM32 firmware implementing [mcu-bridge v1](docs/protocols/mcu-bridge.md):
   Ping/Pong plus GPIO loopback is the smallest proof of the dual-brain
   architecture, and [checklist §2](docs/hardware/TEST_CHECKLISTS.md) is already
   written for it.
