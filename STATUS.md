# Project Status

Rolling snapshot of where Platypus One actually is. If this file disagrees with the
code, the code wins and this file is stale.

**Last updated: 2026-11-22**

## Right now

| | |
|---|---|
| **Deadline** | Hackster AU 2027 submission, Dec 20 (target Dec 17). Plan: #60 |
| **Focus** | **Portability**, **fabrication**, **raw debugging**. Anything else is parked ([ARCHIVE.md](ARCHIVE.md)) |
| **Display** | **Frozen: Waveshare 5inch DSI LCD** (SKU 18396, 800×480, Rev 2.2) on the UNO Media Carrier, running on the bench ([DSI_BRINGUP.md](docs/hardware/DSI_BRINGUP.md)) |
| **Compute** | Arduino UNO Q (QRB2210 Linux + STM32U585 MCU) |
| **Sensors** | IMX219 CSI camera (B0393/B0394) via the kiosk's raw path; Pololu VL53L8CX ToF + SparkFun BMI270 + SparkFun Qwiic Button (trigger, Omron D2F-01L) on the MCU's Qwiic bus |
| **Software** | One integration PR (#61) lands the CSI camera, depth view, shell, best-effort captures, ToF + IMU in every capture, trigger = CAPTURE, and the MCU firmware (protocol 2) |

## Bench state (2026-11-22)

- MCU runs firmware **0.2.0** (trigger + IMU, protocol 2), built and flashed on the board.
  The previous 0.1.0 image is kept and restorable with `~/bin/flash-mcu` on the board.
- Qwiic chain: UNO Q → BMI270 → Pololu ranges cleanly (0 I²C retries, 5 Hz). Adding the
  **Qwiic Button board jams the bus** at both 400 and 100 kHz (every address times out);
  isolation in progress (#50). The SparkFun pull-up jumpers are not cut yet and the
  switch is not fitted (gate T1, [TRIGGER_IMU.md](docs/hardware/tof_unoq/TRIGGER_IMU.md)).
- Board kiosk is still the Oct 18 build; it moves to the integration build once #61 lands.
- Power, one state measured: kiosk + camera + ToF = 5.03 V × 0.46 A = 2.27 W
  ([BENCH_LOG_2026-11-22.md](docs/hardware/tof_unoq/BENCH_LOG_2026-11-22.md)).
- Away from home Wi-Fi the board starts its own access point after 2 min (board-local
  setup, not in this repo); the offline runbook lives on the board as `~/OFFLINE_TODO.md`.

## Focus areas and next actions

**Portability**
- Power states still to measure: idle, boot peak, depth view, full brightness (#50).
- Battery direction (not locked): one Li-ion 18650 + charger + 5 V boost (≥ 2 A), about 4.5 h at
  the measured 2.27 W. Wiring options are being compared before anything is ordered.

**Fabrication**
- Enclosure in Fusion (source of truth); a Fusion `.f3d` must land in `CAD/` (#60).
- UNO Q mounting confirmed with the printed test stand (`tools/fixtures/`); one post needs a
  flat on the header side.
- Slot widths for the camera and ToF boards: design 1.3 / 1.2 mm (prints ~0.1 mm undersize).
- Open: enclosure process (PCBWay CNC vs printed shell) and PCB scope (#60).

**Raw debugging**
- MCU line protocol and logger: `firmware/tof_unoq_vl53l8cx/`, `tools/tof_unoq/`
  (`tof_log.py --send PERIPH|"I2CHZ 100"`, `imu.csv`, bench gates).
- Camera: `tools/camera_characterize/`, the kiosk's raw SRGGB10 capture with `raw.raw10` + `raw.json`.
- Display: `tools/display_probe/`; bring-up record in `docs/hardware/DSI_BRINGUP.md`.
- Bench session script: `tools/bringup/bench_session.sh`.

## Execution rules

1. Finish the enclosure and internal packaging.
2. Keep the hardware to what the submission needs: UNO Q, 5" display, camera, ToF, IMU,
   trigger, battery.
3. Order anything with a lead time early enough that it cannot block December.
4. Build one complete physical engineering measurement workflow.
5. Generate test data, photos/video and documentation while building.
6. Keep stretch features out of the critical path.

**Scope test:** if a task does not move the physical `Point → Measure → Display → Save`
loop or the submission closer, it waits until after Dec 20.

## Calibration is not the product

Engineering Scout must give a useful result under ordinary bench conditions (even,
uneven and shadowed light, exposure changes, rotated parts) and an honest refusal with
retry guidance when the evidence is not enough. The in-frame reference is a scale
anchor, not a calibration project. See [Dream Lab MVP](docs/contest/DIGIKEY_ENGINEERING_SCOUT_MVP.md).

## Evidence discipline

Every physical session leaves durable evidence: source images, observation JSON, test
condition and lighting, pass/fail or measured error, setup photos when useful, and the
blocker and next action. Keep synthetic tests, reported bench results and archived
evidence distinct. The repo tells the build story as it happens.
