# tof_unoq_vl53l8cx — VL53L8CX on the UNO Q Qwiic bus (MCU sketch)

Runs on the UNO Q's **STM32U585** (core `arduino:zephyr`, FQBN `arduino:zephyr:unoq`).
The Qwiic connector is STM32 I2C4 (PD13 SDA / PD12 SCL), exposed by the core as `Wire1`.
Linux cannot reach this bus, so the driver lives here and streams results to Linux through
the Arduino Bridge `Monitor`. Wiring and gates: [docs/hardware/tof_unoq](../../docs/hardware/tof_unoq/README.md).

**Status:** compiles for the real target (`arduino:zephyr@1.0.0`: 195,856 B flash (24 %),
51,852 B RAM (19 %)). Its Arduino-independent headers are host-tested
(`tools/tof_unoq/tests`), including ST's `vl53l8cx_is_alive()` running through this I/O
layer against a simulated register file. **It has not run on hardware.**

| File | Role |
|---|---|
| `tof_unoq_vl53l8cx.ino` | bring-up state machine, ranging loop, commands |
| `tof_i2c_platform.h` | bounded, chunked register I/O for Zephyr `Wire` (retries capped, errors reported) |
| `tof_proto.h` | CRC-16 line protocol shared with `tools/tof_unoq/tofproto.py` |
| `sketch.yaml` | pinned core + library versions (`arduino-cli --profile unoq`) |

Why not ST's Arduino C++ class: its `IO_Read()` retries forever if the sensor does not
ACK, and it assumes a repeated start that Zephyr `Wire` does not issue. Only ST's ULD C API
(from the same library, `VL53L8CX_1.3.0`) is used, unchanged.

## Build

On any PC with [arduino-cli](https://arduino.github.io/arduino-cli/) ≥ 1.0:

```sh
arduino-cli core update-index
arduino-cli core install arduino:zephyr@1.0.0
arduino-cli compile --profile unoq firmware/tof_unoq_vl53l8cx     # installs the pinned libraries
```

Pinned dependencies (`sketch.yaml`): Arduino_RouterBridge 0.4.3 (with Arduino_RPClite
0.3.1, MsgPack 0.4.2, DebugLog 0.8.4, ArxContainer 0.7.0, ArxTypeTraits 0.3.2) and
STM32duino VL53L8CX 2.1.0.

## Flash

The core uploads with `remoteocd` over ADB on the USB-C link, writing the Zephyr loader and
the sketch (`platform.txt`). With the UNO Q connected by USB-C:

```sh
arduino-cli board list                       # the UNO Q appears with FQBN arduino:zephyr:unoq
arduino-cli upload --profile unoq -p <port-from-board-list> firmware/tof_unoq_vl53l8cx
```

Arduino App Lab can upload the same sketch instead. *These upload commands come from the
core's upload recipe. They were not executed in this work (no board attached).*

## Run

1. Power off. Wire per the README. Power on.
2. On the UNO Q, **start the logger first** (the sketch waits for a monitor client before it
   touches the bus, so the whole bring-up is recorded):
   ```sh
   python3 tools/tof_unoq/tof_log.py --out ~/tof-runs/first --frames 20 --label first-light
   ```
3. Expected sequence in the logger output:
   `HELLO` → `BUS_BEGIN` → `SCAN count=1 addrs=0x29` → `ACK` → `ID_OK device_id=240 revision_id=12`
   → `INIT_START` → `INIT_OK ms=<a few thousand>` → `CFG zones=64 hz=5 …` → `RANGING_START` → frames.

Commands the logger (or `nc 127.0.0.1 7500`) can send, one per line: `STATUS`, `STOP`,
`START`, `RES 4|8`, `HZ <n>`, `SCAN`. RES/HZ/SCAN act only while stopped.

## Failure signatures

| Event (level E) | Meaning | First check |
|---|---|---|
| `SCAN count=0`, `NO_ACK` | nothing answers on Wire1 | power off; cable mapping; SPI/I2C shunt; VIN at carrier |
| `ID_FAIL device_id=…` | something answers but is not a VL53L8CX (0xF0/0x0C expected) | other device at 0x29; bus integrity |
| `INIT_FAIL uld_status=255` | comms failed during the 84 KB upload (UM3109: 255 = I2C timeout) | rails during INIT; cable length; scope |
| `CFG_FAIL uld_status=127` | invalid setting (e.g. >15 Hz at 8×8) | command values |
| `DATA_TIMEOUT` (W) ×3 → `RANGING_STALLED` | no data-ready for 3 frame periods | AVDD/VIN under load |
| `READ_FAIL` | a frame read failed | bus integrity |

Every error event includes `i2c_tx / i2c_retries / i2c_failures / i2c_last_fail_reg /
i2c_last_fail_stage` (1 write, 2 read-index, 3 read-data, 4 short read).

## Recovery

* **Sensor reset** requires removing its supply (UM3109 §4.2). The Qwiic 3.3 V cannot be
  switched, so: power the UNO Q off, wait 10 s, power on, then restart the logger.
  (`START` after an error re-runs bring-up but does not reset the sensor.)
* **Logger cannot connect:** `sudo systemctl restart arduino-router`, then retry.
* **Nothing arrives:** close any other monitor client (App Lab serial monitor), re-upload the
  sketch. To rule out the ToF completely, upload Arduino_RouterBridge's `monitor` example.
* **Remove this sketch:** upload any other sketch (e.g. File → Examples → Basics →
  BareMinimum) with the same upload command.
