# Dream Lab evidence index — September 30 submission

**Submission:** completed September 30, 2026, confirmed by Matthew on October 1.
**Archive state:** partial. The final submitted text/URL, actual tested SHA,
raw capture/JSON pairs and physical result media are not yet available here.
This index preserves accessible evidence without inventing missing records.

## Preserved sources

| Item | Evidence type | Source / boundary |
|---|---|---|
| [Later write-up snapshot](WRITEUP_BRANCH_SNAPSHOT.md) | Exact working-draft text, including reported caliper tables | `claude/dreamlab-writeup` at **48e028a4749d5c2dc33a844d56f07affb59d33ea**, file `docs/contest/DREAMLAB_MAKERIO_POST.md`. Not verified as final published text. |
| [Original merged draft](../../DREAMLAB_MAKERIO_POST.md) | Historical contest draft | #31 merge at `d64cad1`; its M6 label for the real screw was wrong, and its session/export content was staged. |
| [Carrier FFC photo](../../../media/hardware/dsi-cable-carrier-end.jpg) / [panel FFC photo](../../../media/hardware/dsi-cable-panel-end.jpg) | Physical setup photographs | Working assembly photographed September 30; #26 records the orientation. These prove cable layout, not a particular measurement or scan ID. |
| [Kiosk preview](../../media/scout-kiosk-preview.png) / [result card](../../media/scout-kiosk-card.png) | **Synthetic** offscreen screenshots generated on the UNO Q | #28's synthetic M8 scene; not photographs of a real screw and not physical accuracy evidence. |
| [Manifest](manifest.json) | Source inventory, hashes and explicit gaps | Null/unknown entries mean evidence has not been recovered. |

The snapshot is copied byte-for-byte. Read it with this index: embedded
claims about staged sessions/classes or future corrections are not accepted
baseline capabilities merely because they appear in a draft.

## What is reported, and what it establishes

| Report | Recorded facts | Boundary |
|---|---|---|
| September 29 first light | Real webcam → touch/result/save loop reported. Screw measured **49.6 × 12.1 mm**, against **44.45 mm overall length / 9.48 mm head diameter** | Workflow ran; large over-read. Raw paired files, lighting details, result photograph and tested SHA are still missing. |
| Object identity correction | Matthew identified the physical test screw as **1/4-20 UNC**; later draft gives 6.3 mm caliper thread width and 6.35 mm standard major diameter | M6/M8 were inferences, not ground truth. Standard dimensions are not substituted for caliper readings. |
| September 30 focused, tilted screw | Draft records on-device head **10.17 / 10.45 mm**, shank **6.72 / 7.30 mm**, length **52.9 / 55.9 mm** | Reported numbers, not a complete acceptance test; the two captures have no preserved scan-ID mapping here. |
| Square-corner correction / thread pitch | Draft records corrected head **9.73 / 9.62**, shank **6.08 / 6.20**, length **49.3 / 50.4 mm**, and pitch **1.284 / 1.230 mm** vs standard **1.270 mm** | **Offline analysis of the same frames**, not merged on-device output; code/method and raw inputs are not archived here. Do not adopt a tuning change from this table alone. |
| OV5640 camera-board scans labelled 0045–0047 | Reported width **24.84 / 24.88 / 24.43 mm**; offline corrected **23.31 / 24.45 / 23.69 mm**; calipers **22.83 mm** | Raw JSON/images absent. Length deliberately omitted because the headers extend beyond the board. |

Focus, lighting, tilt and object height are proposed explanations for the
over-read; these notes do not establish a general correction or accuracy
claim. The snapshot's plane-corrected results and thread-pitch analysis
must stay separate from original on-device JSON. The baseline's synthetic
21-case results and #35's staged 24-case results must also stay separate.

## Recover the final bundle before changing the board checkout

On the UNO Q, in the **existing contest checkout** (historically
`~/PlatypusOne`), stop any running kiosk so completed records are copied
consistently. This does not switch git branches, flash firmware or recompute
an observation. It ends the current kiosk capture session and restores its
USB device role; restarting the kiosk is a separate action.

```bash
set -euo pipefail
cd "$HOME/PlatypusOne"
test -d observations
if systemctl cat platypus-kiosk.service >/dev/null 2>&1; then
  sudo systemctl stop platypus-kiosk.service
fi
EVIDENCE_DIR="$(mktemp -d "$HOME/dreamlab-submission-evidence.XXXXXXXX")"
cp -a observations "$EVIDENCE_DIR/observations"
if [ -d sessions ]; then
  cp -a sessions "$EVIDENCE_DIR/sessions"
fi
git rev-parse HEAD > "$EVIDENCE_DIR/checkout-at-collection.txt"
git status --porcelain > "$EVIDENCE_DIR/checkout-state-at-collection.txt"
uname -a > "$EVIDENCE_DIR/environment-at-collection.txt"
date -u > "$EVIDENCE_DIR/collected-at.txt"
sudo journalctl -u platypus-kiosk.service -b --no-pager > "$EVIDENCE_DIR/journal-at-collection.log"
printf 'Preserved raw files in %s\n' "$EVIDENCE_DIR"
```

The checkout SHA and current boot journal identify **collection time**, not
necessarily the September 30 tested binary or its source. Preserve original
frames, JSON and session files unchanged; label any later captures by their
actual timestamps/commit rather than calling the whole directory contest-night proof.

Add the actual tested SHA/build log, capture-linked lighting/focus/placement
notes, calibration check, same-measurand caliper truth, final posted URL/text,
and photographs/video or their durable references to the bundle. Use
[the run-record template](../../../hardware/SCOUT_BASELINE_RUN_RECORD.md)
with unresolved values left explicit. Then hash and copy the complete bundle:

```bash
(
  cd "$EVIDENCE_DIR"
  find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS
  sha256sum -c SHA256SUMS
)
tar -C "$(dirname "$EVIDENCE_DIR")" -czf "$EVIDENCE_DIR.tar.gz" "$(basename "$EVIDENCE_DIR")"
sha256sum "$EVIDENCE_DIR.tar.gz" > "$EVIDENCE_DIR.tar.gz.sha256"
```

Copy both archive and checksum off the board and verify the copied archive.
Only then update this manifest with durable paths and actual tested-source
identity. A final submission tag waits on that identity. Keep #32 open until
the missing evidence and [native baseline replay](../../../hardware/UNO_Q_SCOUT_BASELINE.md)
are recorded.
