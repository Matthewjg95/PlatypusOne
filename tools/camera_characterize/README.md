# tools/camera_characterize

The camera evidence pipeline for the IMX219 bench-off (#40). It goes from
physical captures to a traceable report and a three-role decision gate.
Design and limits: [docs/hardware/CAMERA_CHARACTERIZATION.md](../../docs/hardware/CAMERA_CHARACTERIZATION.md).

**No camera result exists yet.** `tests/` uses synthetic fixtures, which test
software behaviour and are never camera evidence.

```sh
pip install -r tools/camera_characterize/requirements.txt          # host analysis only
cmake --build build --target scout_measure                         # Scout analyzer bridge
cc=tools/camera_characterize/camera_characterize.py

python3 $cc plan                                                   # the capture matrix
python3 $cc init <exp>/datasets/b0394-20261007 --camera B0394 --dataset-id b0394-20261007
python3 $cc capture <exp>/datasets/b0394-20261007 \
    --command 'tools/camera_characterize/capture_raw.sh {out}'     # on the UNO Q (stdlib only)
python3 $cc validate <exp>                                         # exit 1 on errors
python3 $cc all <exp>                                              # analyze + compare + report
python3 -m unittest discover -s tools/camera_characterize/tests -v
```

| Path | Role |
|---|---|
| `camchar/contract.py` | manifest schema + validation (stdlib) |
| `camchar/plan.py`, `matrix_spec.json` | capture matrix, dataset init, coverage (stdlib) |
| `camchar/capture.py`, `capture_raw.sh` | guided board-side capture (stdlib + v4l-utils). The script is **untested on hardware** |
| `camchar/images.py` | raw10p / raw16 / ABGR8888 / YUYV / PNG / PNM loaders (read-only) |
| `camchar/metrics.py` | exposure, region sharpness, edge rise |
| `camchar/calibration.py`, `camchar/target.py` | ChArUco calibration; printable target |
| `camchar/geometry.py` | centre vs edge residuals |
| `camchar/repeatability.py` | Scout measurements via `tools/scout_measure`, stats, accuracy |
| `camchar/decision.py`, `decision_criteria.json` | per-camera metrics, three-role gate. Criteria fixed before data |
| `camchar/report.py` | Markdown report + PCB/Fusion handoff block |
| `templates/experiment.json` | experiment manifest to copy |

Bump `ANALYSIS_VERSION` (`camchar/__init__.py`) whenever a change can alter a number.
