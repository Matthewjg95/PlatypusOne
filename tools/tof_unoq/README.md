# tools/tof_unoq — Linux side of the UNO Q + VL53L8CX bring-up

Python 3 standard library only. Runs on the UNO Q's Debian (the monitor port is
`127.0.0.1:7500`). Wiring, gates and sources: [docs/hardware/tof_unoq](../../docs/hardware/tof_unoq/README.md).
MCU sketch: [firmware/tof_unoq_vl53l8cx](../../firmware/tof_unoq_vl53l8cx/README.md).

| Tool | What it does |
|---|---|
| `tof_log.py` | connects to the router's monitor port. Writes `raw.log` (every line, OK/BAD, host CLOCK_MONOTONIC + REALTIME), `events.csv` (trigger edges included), `frames.csv`, `zones.csv`, `imu.csv`, `session.json`. `--send CMD` passes extra sketch commands (`PERIPH`, `I2CHZ 100`). Exit codes: 0 frames, 2 no frames (with a diagnosis), 3 connect failed, 4 output exists |
| `analyze.py` | `analysis.json` + `analysis.md`: frame/arrival intervals, I²C read time, MCU→host latency envelope, per-zone validity and medians, central 2×2 median vs the operator's reference. Raw values only. No correction, no pass/fail |
| `coacq.py` | independent camera + ToF capture. Per-frame mode (any capture command, host-time bracket) or stream mode (`v4l2-ctl --verbose`, V4L2 `ts-monotonic` buffer timestamps). Writes `pairs.csv` (nearest ToF frame per camera frame, dt) and `coacq.json` |
| `tofproto.py` | line protocol + CRC; ST UM3109 target-status table and confidence classes |
| `v4l2ts.py` | parser for `v4l2-ctl` per-buffer lines (format from v4l-utils `print_concise_buffer`) |
| `diagrams/gen_diagrams.py` | regenerates `docs/hardware/tof_unoq/diagrams/*.svg` (`--png` previews via headless Chromium) |

## Use

```sh
python3 tools/tof_unoq/tof_log.py --out ~/tof-runs/E-500mm-a --frames 100 \
    --label 500mm-white-card --ref-distance-mm 500 --target "white card, matte" --operator me
python3 tools/tof_unoq/analyze.py ~/tof-runs/E-500mm-a
python3 tools/tof_unoq/coacq.py --out ~/tof-runs/F-01 --camera-frames 20 \
    --camera-cmd 'tools/camera_characterize/capture_raw.sh {out}'
```

`--res 4|8` and `--hz N` reconfigure after start (STOP → RES/HZ → START). `--stop-on-exit`
stops ranging when the logger exits.

## Timing honesty

* `mcu_us_*`: the sketch's 64-bit-extended `micros()`.
* `host_*_ns`: Linux arrival time. It **lags acquisition** by the I²C read-out plus the
  Bridge UART, the router and TCP. `analyze.py` reports the lower envelope of host − MCU
  time and the spread above it. That is an estimate, not a synchronisation.
* `coacq.py` pairs by nearest Linux CLOCK_MONOTONIC time and says so in its manifest.
  Camera time is the V4L2 buffer timestamp only when v4l2-ctl reports `ts-monotonic`.
  Otherwise it is the midpoint of the capture command, with its half-width as the
  uncertainty.

## Tests

```sh
python3 -m unittest discover -s tools/tof_unoq/tests -v
```

All inputs are **synthetic** (generated lines, a fake monitor TCP server, a fake camera
command, a simulated register file). They test software behaviour, never the sensor. The
C++ host test compiles the sketch's `tof_i2c_platform.h`, `tof_proto.h` and
`qwiic_peripherals.h` with `g++` (the trigger and IMU against simulated register files).
The vendored BMI270 config is checked against Bosch's SHA-256. When
ST's ULD sources are available (`TOF_ULD_SRC`, or the Arduino library folder) it also runs
the real `vl53l8cx_is_alive()` through that I/O layer.
