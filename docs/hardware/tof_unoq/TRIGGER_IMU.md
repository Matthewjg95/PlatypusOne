# Trigger and IMU on the ToF's Qwiic bus — wiring and bench gates

**Only the human operator closes a gate.** Nothing here has been run on hardware yet; the
firmware has only been exercised against simulated register files
(`tools/tof_unoq/tests/host/fake_qwiic.h`). Same rules as the
[ToF bench checklist](BENCH_CHECKLIST.md): record what you see, STOP on surprises.

Parts (bought Oct 2026, issue #50):

| Part | Role | Address |
|---|---|---|
| SparkFun 6DoF IMU Breakout — BMI270 (Qwiic) | IMU: tilt at every capture | 0x68 (ADR jumper open) |
| SparkFun Qwiic Button breakout BOB-15931 (no switch fitted) | reads the trigger switch, debounces, latches clicks | 0x6F |
| Omron D2F-01L microswitch | the trigger | — |
| Existing Pololu #3419 + Qwiic-to-socket cable | ToF, last in the chain | 0x29 |

## 1. Chain

```
UNO Q Qwiic (Wire1 / I2C4) ─► BMI270 board ─► Qwiic Button board ─► Qwiic-to-socket cable ─► Pololu VL53L8CX
```

The UNO Q has one MCU Qwiic port, so everything daisy-chains. The Pololu goes last because
its cable ends in 0.1″ sockets, not a Qwiic plug. Two extra short Qwiic cables are needed
(the SparkFun boards each have two Qwiic connectors; either one is in or out).

## 2. Trigger switch

D2F-01L terminals are COM, NO and NC. Use **COM and NO** only: open at rest, closed when
pressed. Solder them to the two sides of the breakout's button footprint. The footprint's
pads are internally paired; before soldering, beep pad pairs with no switch fitted and pick
two pads that are **not** connected to each other at rest.

## 3. Pull-ups

The UNO Q already pulls SDA/SCL up. Each SparkFun Qwiic board adds its own pull-ups behind a
cuttable `I2C` jumper; three sets in parallel load the bus beyond what 400 kHz needs. Cut
the jumper on both SparkFun boards, and **verify by measurement, not by the cut**:

| # | Measure (everything unpowered, meter in ohms) | Expect | Record |
|---|---|---|---|
| P1 | SDA ↔ 3V3 at the UNO Q Qwiic connector, nothing plugged in | the UNO Q's own pull-up | Ω |
| P2 | Same, each SparkFun board alone (jumper cut), on its own Qwiic pins | open / very high | Ω |
| P3 | Same at the UNO Q with the whole chain plugged in | ≈ P1 (the Pololu adds its level-shifter side, record it) | Ω |
| P4 | Repeat P1–P3 for SCL | same pattern | Ω |

**STOP** if P2 reads a few kΩ (jumper not fully cut) or P3 is far below P1 for no reason
you can name.

## 4. Gates

Firmware: `tof_unoq_vl53l8cx` **0.2.0**, protocol **2**. Flash as before, then run the
logger on the UNO Q:

```bash
python3 tools/tof_unoq/tof_log.py --out ~/tof-runs/T-$(date +%Y%m%dT%H%M%S) --duration 120
```

The sketch prints numbers in **decimal**: 0x5D = 93, 0x24 = 36, 0xA8 = 168, 0xE8 = 232,
0x0E = 14.

### Gate T1 — unpowered

| # | Check | Expect |
|---|---|---|
| T1.1 | Beeper across the two switch wires at the breakout pads, lever released / pressed | silent / beep (both, every time, 10 presses) |
| T1.2 | Pull-up table §3 | recorded, no STOP |
| T1.3 | GND, 3V3, SDA, SCL continuity from the UNO Q Qwiic connector to the Pololu J1-4/J1-3/J1-5/J1-6 through the whole chain | continuity on all four; no short between any two |
| T1.4 | Photos of both SparkFun boards (both sides) with the cut jumpers visible | photos |

### Gate T2 — bring-up (logger output)

| # | Line | Go |
|---|---|---|
| T2.1 | `HELLO` | `sketch=tof_unoq_vl53l8cx 0.2.0`, `proto=2` |
| T2.2 | `EVT … SCAN` | `count=3`, `addrs=0x29 0x68 0x6f` |
| T2.3 | `EVT … TRIGGER_OK` | `id=93`, `debounce_ms=10`, `pressed=0` (lever released) |
| T2.4 | `EVT … IMU_OK` | `stage=ok`, `chip_id=36`, `internal_status=1`, `acc_conf=168 acc_range=1 gyr_conf=232 gyr_range=1 pwr_ctrl=14 err_reg=0`; record `upload_ms` |
| T2.5 | ToF `INIT_OK`, `CFG`, then `FRM` lines | unchanged from the ToF-only bring-up |

A failure names its stage. `TRIGGER_FAIL` with `id=0` and `comms_status=255` means nothing
answered at 0x6F. `IMU_FAIL stage=chip_id` means nothing at 0x68 (or the ADR jumper is
closed: 0x69). `IMU_FAIL stage=init_status` means the part refused the config. Power-cycle
the whole board before retrying (USB-C unplug, not just a reset); `--send PERIPH` re-runs
only the trigger and IMU bring-up.

### Gate T3 — function

| # | Do | Go |
|---|---|---|
| T3.1 | 10 deliberate presses | 10 `TRIGGER edge=press` and 10 `edge=release`; final `presses=10`; no extra edges (bounce) |
| T3.2 | 10 very fast taps | `presses` rises by 10; some may show `src=click_latch`, which is expected |
| T3.3 | Hold the trigger 3 s | one press, one release, nothing in between |
| T3.4 | IMU board flat and still on the bench, 30 s | console `[IMU]` lines: one axis near ±1000 mg, the others near 0; `|a|` close to 1000 mg. Record the three axes and `|a|` — no accuracy claim |
| T3.5 | Rotate the board 90° about each axis in turn | the ~1000 mg moves to the axis now pointing down; record which axis and sign for each pose (this becomes the mounting table later) |
| T3.6 | Still, then a quick twist | gyro near 0 dps at rest (record the offsets), a clear spike on the twist axis |

### Gate T4 — bus integrity with everything running

| # | Do | Go |
|---|---|---|
| T4.1 | Log 5 minutes with ranging at the default 5 Hz, pressing the trigger now and then | `session.json`: `crc_or_format_rejects` 0, ToF frame rate as before, `peripheral_errors` empty; last `HB` has `i2c_failures=0` and `periph_i2c_failures=0` |
| T4.2 | Scope SCL (B&K 2194, or AD3 Pro protocol decode) during the run | clean edges; note any clock stretching (SCL held low by a slave after an ACK). The Qwiic Button is a small MCU and is the likely stretcher |
| T4.3 | Only if T4.1 shows failures: `--send STOP --send "I2CHZ 100" --send START`, repeat T4.1 | failures gone at 100 kHz (record both runs) |

## 5. What the records mean

* `EVT … TRIGGER edge=press|release presses=<n> src=level|click_latch`. `level` edges come
  from polling `isPressed` every 20 ms. `click_latch` edges are reconstructed from the
  button's latched `hasBeenClicked` flag when a whole click fell between two polls; their
  time is the poll time.
* `IMU mcu_us, seq, sensortime, ax, ay, az, gx, gy, gz[, t_cc=]` at 10 Hz. Acceleration is in
  mg (±4 g range), angular rate in mdps (±1000 dps), on the **BMI270's own axes**. No
  mounting transform and no calibration are applied, and the Bosch gyro cross-sensitivity
  compensation is not applied either. Tilt relative to the camera needs the mounting table
  from T3.5.
* The trigger and the IMU are polled in every sketch state once a monitor client is
  connected, so the trigger still works if the ToF bring-up fails.

Sources: SparkFun Qwiic Button library `registers.h` (status bits, ID 0x5D); Bosch
BMI270_SensorAPI v2.113.0 (`bmi2_defs.h` register map, `bmi2.c` init sequence, the vendored
8 KB config in `firmware/tof_unoq_vl53l8cx/bmi270_config.h`, SHA-256 checked by the tests).
