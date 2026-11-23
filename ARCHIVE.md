# Archive index

Simplify, don't forget. This is the list of work that was parked or removed from
`main` so the repository could focus on the Dec 20 Hackster submission (#60):
**portability, fabrication, raw debugging**. Nothing here was deleted from history.

- **Branches** are kept as tags named `archive/<branch>` at their exact last commit.
  Restore one with `git push origin archive/<branch>:refs/heads/<branch>`.
- **Closed PRs** keep their full diff and discussion on GitHub and can be reopened.
- **Parked issues** are closed with the label [`after-dec-20`](https://github.com/Matthewjg95/PlatypusOne/issues?q=label%3Aafter-dec-20).
- **Files removed from `main`** on 2026-11-22 are listed at the end; every one of them is
  intact in tag `archive/claude/fervent-ptolemy-yx6ers` (`git show archive/claude/fervent-ptolemy-yx6ers:<path>`).

## Decisions taken in this pass (2026-11-22)

- **Display frozen at the Waveshare 5inch DSI LCD** (SKU 18396, 800×480, Rev 2.2) on the UNO
  Media Carrier, the panel running on the bench. The 3.5", 4" and 4.3" work is parked below.
- **Software lands as one integration PR** (#61) instead of the 9-PR stack.

## Parked work: revive when

| Item | What it is | Where | Revive when |
|---|---|---|---|
| Sketch intent resolver | Contour → reviewable sketch proposal → Mesh2CAD export | PR #47, issue #37, tag `archive/claude/sketch-intent-resolver` | Week 3 of #60, only if the core capture loop is done early (creativity points) |
| Rev A-SM1 sheet-metal chassis | Sheet-metal requirements and Fusion parameters for the smaller display | PR #43, tag `archive/claude/rev-a-sm1-display` | Post-contest metal enclosure; the parameter approach carries over to the 5" |
| 4" display gate | 4-DSI-TOUCH-A source audit and compile-only gate | PR #57, tag `archive/automation/compact-display-4in-gate-20261021` | Only if the 5" fails in the enclosure |
| Compact-display selection | 3.5"/4"/4.3" comparison, bench acceptance, mechanical CSV, `tools/compact_display` (installer + restore script) | Removed files below; merged history of #45 | Same as above. The working 5" setup is rebuilt from `docs/hardware/DSI_BRINGUP.md` step 10, not from `restore_5in.sh` |
| Linked / Tab5 / ESP32-S3 display path | ADR-0001's external display, Tab5 plan, ESP32-S3 SUB3 board notes | Removed files below; ADR-0001 marked superseded | Not planned |
| Enclosure mock-ups | 3.5"/4" display dummies and the open-top device tray v1–v5 generators | Tag `archive/claude/enclosure-mockups` | Superseded by the Fusion model; the UNO Q test stand moved to `tools/fixtures/` |
| Fusion ID beef-up script | Sep 2026 Fusion script + STEP for grip/sensor-head ID studies | Tag `archive/claude/platypus-one-fusion-0v6eiu` | Design reference for the final enclosure styling |
| IMX219 bench-off kit | Evidence script and procedure to compare the B0393 and B0394 | PR #42, tag `archive/claude/csi-bench` | If the camera choice changes |
| BOM traceability + BOM slide deck | BOM derived from the Mesh2CAD handoff; component review deck (incl. rotary encoder options) | PR #8, tags `archive/codex/mesh2cad-bom-traceability`, `archive/docs/bom-review-deck` | Week 6 of #60: inputs for the final Hackster BOM |
| AI-01 planar observation | Deterministic planar observation prototype | PR #22, issue #19, tag `archive/feat/ai01-planar-observation` | After Dec 20 |
| Fiducial chit | Removable measurement reference token | Issue #38 | After Dec 20 |
| PSoC E84 + radar evaluation | Bounded evaluation plans | Issue #18 | After Dec 20 |
| Systems-engineering architecture pack | Lightweight architecture pack | Issue #17 | After Dec 20 |
| ShadowScan, viewer, inspection, documentation apps | Placeholder READMEs for future apps | Removed files below; listed in `docs/ROADMAP.md` M3 | After Dec 20 |
| PolarFire Reflex proposal | Proposal + block diagram for a separate contest | Removed files below | Not planned |

## Superseded (closed, kept for reference)

| Item | Superseded by | Where |
|---|---|---|
| Repo housekeeping (Aug) | Later work and this pass | PR #20, tag `archive/chore/housekeeping` |
| Executable ToF bring-up (codex) | The bench-proven VL53L8CX path (#49, 2026-10-18 ranging) | PR #21, tag `archive/codex/issue-16-tof-bringup` |
| Depth-sensor evidence gate | VL53L8CX chosen and ranging | PR #15, tag `archive/codex/depth-sensing-evidence-gate` |
| Board-button `--watch` capture | The hardware trigger in the kiosk (#58/#59) | Tag `archive/claude/scout-button-trigger` |
| Scout capture slice | The Engineering Scout capture path in `main` | PR #11, tag `archive/feat/scout-capture-slice` |
| Packaging reset (smaller display) | The 5" freeze | Issue #41 |

Branches whose work is fully in `main` or in the integration PR (#61) were deleted after
tagging; every one still has its `archive/` tag.

## Files removed from `main` (2026-11-22)

```
tools/compact_display/                         (installer, restore script, panel files, tests)
.github/workflows/display-tools.yml            (CI for tools/compact_display)
docs/hardware/COMPACT_DISPLAY_BENCH_ACCEPTANCE.md
docs/hardware/COMPACT_DISPLAY_SELECTION.md
docs/hardware/DISPLAY_COMPARISON.md
docs/hardware/DSI_PANEL_CANDIDATES.md
docs/hardware/ESP32-S3-LCD-EV-BOARD-SUB3_V1.3.md
docs/hardware/compact_display_mechanical.csv
docs/roadmap/TAB5_DISPLAY_PROTOTYPE.md
apps/shadowscan/README.md
apps/viewer/README.md
apps/documentation/README.md
apps/inspection/README.md
docs/contest/POLARFIRE_REFLEX_PROPOSAL.md
docs/contest/POLARFIRE_REFLEX_SYSTEM_BLOCK_DIAGRAM.svg
```
