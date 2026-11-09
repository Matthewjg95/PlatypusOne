# Post-Dream-Lab stabilization — issue #32

Audit date: **2026-10-01**. Dream Lab was **submitted September 30**, confirmed
by Matthew. This is a baseline/evidence task, with no new product scope or
measurement tuning.

## Repository and dependency order

The audit started at `main` **d64cad1340142c25c822e57098f53e0fa8f64a4b**.
The kiosk baseline is now **ea5e82a36c21de395f347425b8bfa5a7e1638fc2**.

| PR | Audited state / action | Dependency and remaining gate |
|---|---|---|
| #25 | Merged October 1 after review, existing green checks, and local integration tests | Independently justified refusal of clipped blobs; its September 21 defect is documented. No new threshold or dimensional correction was added. |
| #26 | Already merged September 30 | Correct 800×480 panel identity, FFC photos and bring-up notes. Later bench result supersedes the earlier cable/image hypotheses. |
| #27 | Already merged September 30 | KMS display/probe dependency for the kiosk. Original PR description stops short of on-glass proof; later notes report that proof. |
| #30 | Already merged September 30 | Independent `<cstdint>` self-containment fix and pinned formatting; required before the session/export headers build reliably. |
| #31 | Initial draft already merged September 30 | Branch later advanced to `48e028a`; that newer text is archived separately, not silently treated as main or submitted copy. |
| #28 | Merged October 1 after repairing conflicts and passing both CI jobs | Main's `display_probe` and the kiosk CMake target are both retained; lighting/speck and clipped-blob tests are both retained. A synthetic offscreen capture smoke check now runs in Linux CI. |
| #29 | Draft, retargeted to `main` and reconciled at `48fdb9a` | Retained `--sessions` and resolved the restart-comment conflict; local checks and fresh Windows/Linux CI pass. A preserved native CAPTURE → FINISH/export run remains the merge gate. |
| #35 | Open, still stacked on `claude/scout-session` (#29); conflicts with its updated base | Positive-evidence classes, UNC/metric alternatives and tilt evidence are staged. Reconcile separately, review against preserved real frames, and require fresh CI; do not merge it ahead of #29 or tune it using narrative tables alone. |

The remaining order is **#29 → #35**, after the baseline's native replay and
evidence preservation. Neither draft session capability nor a green host
classifier battery establishes physical acceptance.

## CI: actual failures, not stale red icons

| Run | Actual result / cause | Resolution |
|---|---|---|
| [36708982063](https://github.com/Matthewjg95/PlatypusOne/actions/runs/36708982063), first #30 head `e056a86` | Windows formatting gate failed in `Types.hpp`; Linux build/tests passed | `cc340b3` formatted the header; [36774691841](https://github.com/Matthewjg95/PlatypusOne/actions/runs/36774691841) passed before #30 merged |
| [36664743898](https://github.com/Matthewjg95/PlatypusOne/actions/runs/36664743898), earlier #29 head `99060f4` | GCC build: `std::uint32_t` unavailable because `Types.hpp` omitted `<cstdint>` | Session commit `3db1e0e` fixed it; [36665991097](https://github.com/Matthewjg95/PlatypusOne/actions/runs/36665991097) passed at `74c75ac`; independent fix also merged through #30 |
| [36775210722](https://github.com/Matthewjg95/PlatypusOne/actions/runs/36775210722), audit-start main `d64cad1` | Both jobs passed | No remaining CI failure at that main head |
| [36894869873](https://github.com/Matthewjg95/PlatypusOne/actions/runs/36894869873), repaired #28 head `fdf759c` | Both Windows/MSVC and Linux/GCC passed, including the new synthetic kiosk smoke check | Merged using the checked head SHA; no bypass of failing checks |
| [36895514298](https://github.com/Matthewjg95/PlatypusOne/actions/runs/36895514298), merged baseline `ea5e82a` | Host CI passed on main after #28 merged | This is the pinned runtime source for the reproduction sequence |
| [36897961677](https://github.com/Matthewjg95/PlatypusOne/actions/runs/36897961677), reconciled draft #29 `48fdb9a` | Both Windows/MSVC and Linux/GCC passed, including synthetic offscreen capture/finish/export | #29 remains draft pending a preserved native session/export run |

Canceled intermediate main runs during the rapid September 30 merges were
superseded by the successful latest-head run. Rerunning an old failing head
would not validate current source. #29's old green run remains historical
after its base changed; the reconciled `48fdb9a` now has its own passing checks.
#35's prior green head does not resolve its conflict with the updated #29 base.

Local integration verification used **Linux x86_64 / GCC 13.3**, Release,
asserts enabled by the existing test target, `ctest`, the **21/21 synthetic
battery**, and a saved synthetic YUYV/JSON/preview/card. Pinned
clang-format 18.1.8 checked merged C++ files. Existing conversion warnings
in the link framing header and validation-report arithmetic remain nonfatal;
this was not a warning-clean native UNO Q build.

## Physical claims and preservation

[The evidence index](evidence/dreamlab-2026-09-30/README.md) preserves the
exact later write-up, working FFC photos and source references. It records
the reported first-light over-read and September 30 caliper comparisons,
including their offline-analysis boundary. Raw observation/image pairs,
final tested SHA, complete lighting labels, physical result media and the
published submission link are still missing from accessible repository evidence.

The real screw is **1/4-20 UNC**, not the original M6/M8 inference. Main's
metric-only classifier is not a reliable nominal identifier for that test
part; #35's alternatives are not yet baseline behavior. Corner correction,
height correction and thread-pitch results must not be claimed as this
baseline's on-device outputs.

## Carry-forward register

| Source | Disposition | Unfinished work and home |
|---|---|---|
| #9, September 30 vertical slice | Superseded after submission; closure does not mean all acceptance items passed | #32: archive source/JSON/calibration/conditions/truth/media/tested SHA; native replay and varied-lighting/refusal checks. MCU trigger/illumination remain future hardware work under #34. |
| #23, "TONIGHT" physical pass | Superseded by this procedure and #32 | #32: final evidence, exact SHA, repeat captures/conditions, useful refusal, physical result media and highest-value next action. Its wrong panel profile is removed from the checklist. |
| #29 | Keep draft; main reconciliation and fresh CI complete | Install the session service and preserve one real FINISH/export bundle; review exports separately from the single-capture baseline. |
| #35 | Keep staged behind #29; current-base conflict unresolved | Reconcile its branch and require fresh CI, then review class behavior on preserved source frames with verified object identity; keep offline corrections and on-device output distinct. |
| #12 / #16 / #19 / #33 / #34 | Keep active | Separate Autodesk pitch, ToF, observation, carrier and convergence work; not stale Dream Lab deadline tasks. |

## Remaining completion gates for #32

- [ ] Archive final source frames and their original observation JSON; retain calibration and raw pixel-format/dimension information.
- [ ] Identify the actual submission-night tested SHA, any local changes, and the published post/media references; do not substitute a collection-time SHA.
- [ ] Attach each caliper measurand and lighting/focus/placement condition to its capture ID. Preserve failures as well as successes.
- [ ] Execute [the pinned UNO Q build/launch sequence](../hardware/UNO_Q_SCOUT_BASELINE.md) on the physical rig and preserve its logs plus one real result and one intentional refusal.
- [ ] Record whether session FINISH/export actually ran; leave it unresolved until a real bundle/log is available.

The next action is to preserve the existing board evidence **before**
changing its checkout, then replay the single-capture baseline. Do not create
a `dreamlab-2026-submission` tag until the actual tested source is known.
