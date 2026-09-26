# Tab5 / Pololu #3419 first-ranging scaffold

Read [the electrical procedure](../../docs/hardware/TOF_BRINGUP.md) first.
Standalone, single-loop M5.In_I2C adapter; no competing Wire.begin(), no CAD,
no changes to the MCU bridge or display client. No automatic firmware upload.

## Prepare and compile

From repository root, with Git, Python 3 and Arduino CLI installed:

```sh
python tools/tof_bringup/prepare.py
arduino-cli board listall
arduino-cli core list
arduino-cli lib list
```

Use the installed **M5Stack Tab5** board definition from the official
[Tab5 Arduino setup](https://docs.m5stack.com/en/arduino/m5unified/intro_v2).
Install M5Unified >=0.2.23 and M5GFX >=0.2.30 (Tab5 screen reset fixes).
Record exact resolved versions. The adapter was inspected against M5Unified
`8b63555e7bd9ac71978c55ad868fd1d1d3b2ef3e`; ULD is pinned by prepare.py.
Do not guess a UNO Q FQBN or use its MCU for this sketch.

```sh
# Replace with the Tab5 FQBN reported by your installed board package.
arduino-cli compile --fqbn "$TAB5_FQBN" --warnings all firmware/tof_bringup
# Only Matthew, after saving the existing firmware and checking the board:
arduino-cli upload --fqbn "$TAB5_FQBN" --port "$TAB5_PORT" firmware/tof_bringup
arduino-cli monitor --port "$TAB5_PORT" --config baudrate=115200
```

Enable USB CDC on boot in the board options if required by the selected core.
Preserve the compile/upload output and full serial capture (Arduino IDE or your
terminal logger). Serial commands: `b` scans baseline; `r` starts after the
human rail gate. At reboot it waits again. Do not send `r` without a carrier.

The generated `src/uld/` is ignored and contains unmodified, notice-preserving
ST C sources, not the Arduino TwoWire wrapper. Preparation deliberately refuses
to overwrite an existing dependency directory. Delete that generated directory
explicitly if regeneration is needed. One target per zone is the pinned default.

## Validation boundary

No Tab5 or target toolchain was initially available in the execution workspace.
Source/API review is not a target compile or hardware pass. Record any subsequent
verification below. Mandatory local gate: a successful compile using your exact
Tab5 core before flash. Then perform the procedure's E0/E1 checks.

This scaffold retains status, target count, distance, sigma, signal, ambient,
frame count, uptime and I2C failures. It does not yet log sensor temperature,
IMU sample gaps, touch latency, synchronized exposure time or calibrated pose.
It logs first-target data only and uses status 5 as strict-valid; it does not
silently replace invalid ranges. Those limits remain explicit in evidence.

Verified in the host workspace (2026-09-26): preparation at pinned ULD commit;
`bash tests/tof_bringup/run.sh` compiles ST C sources and the sketch against a
fake M5 header, checks 16-bit register addressing, chunk progression, and stop
cleanup on write failure. This is **not a Tab5 target compile**.
`python -m unittest discover -s tests/tof_bringup -p 'test_*.py'` checks summary
metrics, invalid-as-unresolved behavior, and rejection of truncated/rebooted logs.
