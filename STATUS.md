# Project Status

Rolling snapshot of where Platypus One actually is. Updated as work lands —
if this file disagrees with the code, the code wins and this file is stale.

**Last updated: 2026-08-24**

## Right now

| | |
|---|---|
| **Deadline in focus** | Hackster hardware application, **Sep 7, 2026** (extended from Aug 23) — target submit **Sep 4** |
| **Critical path** | The Fusion `.f3d` first pass. Owner-only work; nothing else in the application is blocked |
| **Hardware in hand** | M5Stack Tab5 (prototype display fixture). No UNO Q, no sensors |
| **Software state** | Builds and runs host-side; no hardware drivers exercised |
| **Display** | Deliberately unfrozen — [ADR-0001](docs/adr/0001-dynamic-linked-prototype-display.md) |

## Milestones

- **M0 Foundation** — complete. HAL, app framework, renderer, host simulator,
  unit tests, architecture docs.
- **M1 Board bring-up** — partial. MCU bridge protocol and Linux driver written
  (untested against hardware); STM32 firmware, camera, sensors, cross-compile,
  and CI outstanding.
- **M2 Runtime maturity** — started. Win32 simulator window and runtime
  geometry landed; dirty rects, event queue, settings store outstanding.
- **M3 Flagship apps** — not started. Six app directories hold READMEs only.
- **M4 Platform opening** — not started.

Detail and tags: [docs/ROADMAP.md](docs/ROADMAP.md).

## Decisions on record

| ADR | Decision |
|---|---|
| [0001](docs/adr/0001-dynamic-linked-prototype-display.md) | Display target stays dynamic; prototype on a linked external display; geometry discovered at runtime |

## Open decisions blocking work

| Decision | Blocks | Notes |
|---|---|---|
| Production panel | Enclosure depth, carrier PCB, `display/driver` | Deliberately deferred by ADR-0001 |
| ToF: VL53L1X vs VL53L8CX | Carrier PCB freeze | ⚖ in [BOM](docs/hardware/BOM.md) |
| IMU: BNO055 vs BMI270 | Carrier PCB freeze, first sensor driver | BNO055 recommended on schedule grounds |
| Camera: UVC OV5640-class vs 13 MP module | Camera driver path | UVC recommended (V4L2 is the simple path) |
| Encoder in the HAL vs MCU-bridge topic | `display/linked` input mapping | Presentation link already reserves the message |
| Espressif carrier identity (Korvo vs LCD-EV-Board SUB3) | Second prototype display option | Panel specs agree; board identity does not |

## Known gaps

- **No LICENSE file.** The contest requires citing licences for third-party
  code, and the repository is public. Needed before the repo link goes on the
  Hackster page.
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
| `feat(sim): make simulated display geometry a runtime choice` | Sim display geometry injection, `--geometry`, launcher row scaling, new geometry test | **Unverified — needs `cmake --build` + `ctest`** |

Please run the host build and tests when convenient and record the result here.

## Next up

1. **[owner]** Fusion `.f3d` first pass →
   [application checklist](docs/contest/HARDWARE_APPLICATION_CHECKLIST.md) §6.
2. **[owner]** Confirm on the live contest page whether the Sep 18 recipients
   date moved with the extension.
3. **[owner]** Run the host build and tests to clear the verification table.
4. `infra/format` + CI + LICENSE — cheap, and they protect the host-sim path
   while attention moves to hardware.
5. `appfw/event-queue`, then `renderer/dirty-rects` — in that order, since
   `display/linked` needs both before it can be wired into the composition root.
6. STM32 firmware implementing [mcu-bridge v1](docs/protocols/mcu-bridge.md):
   Ping/Pong plus GPIO loopback is the smallest proof of the dual-brain
   architecture, and [checklist §2](docs/hardware/TEST_CHECKLISTS.md) is already
   written for it.
