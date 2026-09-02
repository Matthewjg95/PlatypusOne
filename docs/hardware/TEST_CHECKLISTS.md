# Platypus One — Hardware Test Checklists

Work each checklist top to bottom; do not proceed to the next stage until the
current one passes. Record date/initials/notes inline (copy this file per unit
if building more than one). Contest tie-in: photograph each stage — bring-up
photos feed the 20-point documentation score.

## First UNO Q session — status board

The bring-up-brief sequence for the first physical session (UNO Q in hand,
M5Stack Tab5 as the temporary linked display per
[presentation.md](../protocols/presentation.md) and ADR-0001). Update the
Status column as physical testing occurs: `UNTESTED`, `PASS`, `FAIL`, or
`BLOCKED (reason)`. Nothing is claimed compiled or run on the UNO Q until it
actually has.

| # | Step | Detail | Status |
|---|---|---|---|
| 1 | Board/environment verification | Checklist §1 (boot, network, `uname -a`, storage) | UNTESTED |
| 2 | Hello-world | Any trivial program compiles + runs on the Linux side | UNTESTED |
| 3 | Linux-side Platypus executable | On-device native build: cmake + full `platypus_tests` + launcher headless (§2) | UNTESTED |
| 4 | MCU ↔ Linux communication | Flash `firmware/mcu_bridge`; Ping/Pong + GPIO loopback over `/dev/ttyRPMSG0` (§2) | UNTESTED |
| 5 | UNO Q ↔ Tab5 heartbeat | `LinkedDisplay` session: Hello/HelloReply + Ping/Pong over USB CDC | UNTESTED |
| 6 | Tab5 input → UNO Q → response | Touch on Tab5 reaches the app via EventQueue; visible UI response tile returns | UNTESTED |
| 7 | Camera capture | `engineering_scout_capture --device /dev/video0` produces a real observation (§4) | UNTESTED |
| 8 | Scout pipeline on a real image | Analyzer + classifier over a physical fastener beside the 20 mm reference | UNTESTED |
| 9 | Result on Tab5 | Scout result card rendered through `LinkedDisplay` on the Tab5 panel | UNTESTED |

Session prerequisites: a USB serial path between the UNO Q and the Tab5
(cable/role decision is open), the UNO Q Arduino core installed in
arduino-cli for step 4, and the Tab5 display-client firmware for steps 5–9.

## 0. Bench safety / prerequisites

- [ ] Multimeter available and battery-checked
- [ ] ESD-sensitive work is performed on a grounded static-dissipative mat with
      a verified wrist strap/common-point ground (methodology in the private
      planning overlay)
- [ ] Loose ICs remain in shielding/dry packaging until the controlled bench and
      assembly window are ready; record MSL opening time where applicable
- [ ] USB-C supply verified 5.0–5.2 V open-circuit before first connection
- [ ] LiPo visually inspected (no puffing/damage); never charge unattended

## 1. UNO Q first boot (no peripherals)

- [ ] Board powers from USB-C; power LED on; no hot components (touch test after 60 s)
- [ ] Current draw at idle noted: ______ mA (expect < 500 mA)
- [ ] Linux boots; reachable via App Lab / USB gadget / UART console
- [ ] `uname -a` recorded; OS image version noted: ____________
- [ ] Wi-Fi joins network; `ping 8.8.8.8` OK
- [ ] microSD detected, formatted, mounted; write+read 100 MB test file
- [ ] Date/time syncs (NTP)
- [ ] Reboot 3× — boots cleanly every time
- [ ] MCU side alive: stock LED matrix demo (or blink) runs

## 2. PlatypusOS on target

- [ ] Cross-compile (or on-device build) of platypus_launcher succeeds
- [ ] `platypus_tests` passes on target: ____ / ____
- [ ] Launcher runs headless (HostSim fallback) without crash for 10 min
- [ ] MCU bridge: flash protocol firmware; `ping` round-trip < 50 ms over /dev/ttyRPMSG0
- [ ] GPIO loopback: digitalWrite pin → jumper → digitalRead pin reads back both levels
- [ ] analogRead on a divider reads mid-scale ±5%

## 3. Display + touch

- [ ] Display wired per schematic; supply rail correct **before** power-on
- [ ] Backlight lights; full-white and full-black frames show no dead rows/columns
- [ ] RGB565 test pattern: red/green/blue quadrants render correct colors (byte order!)
- [ ] Full-frame present ≥ 15 fps sustained; rate noted: ______ fps
- [ ] Touch: taps in all 4 corners + center report within ±5 px after calibration
- [ ] Launcher UI visible and navigable on the physical panel
- [ ] 30-min soak: no display corruption, no watchdog resets

## 4. Camera

- [ ] Enumerates as /dev/video0 (`v4l2-ctl --list-devices`)
- [ ] 640×480 capture produces a well-exposed frame (save as bring-up artifact)
- [ ] Sustained streaming 30 s without USB dropouts; fps noted: ______
- [ ] ICamera driver: capture() returns valid Frame; startStream callback fires
- [ ] Focus acceptable at 10–30 cm working distance (ShadowScan range)

## 5. Sensors (per sensor: repeat for ToF, IMU, color)

- [ ] I²C address ACKs on bus scan (`i2cdetect` or MCU probe): addr ______
- [ ] Driver registers with ISensorHub; descriptor fields correct (unit, channels)
- [ ] Static sanity: ToF vs tape measure at 100/500/1000 mm within datasheet spec
- [ ] IMU: flat surface reads ~0° pitch/roll; 90° box test within ±1°
- [ ] Color: white paper reads ≈ white balance; red/green/blue cards separate cleanly
- [ ] 1000-sample streaming run: zero dropouts, no NaNs, rate matches requested Hz
- [ ] Inspection app displays the sensor live with no code changes (plugin proof)

## 6. Power & battery

- [ ] Charge board outputs 5.0–5.25 V under 1.5 A load (resistor or board load)
- [ ] System runs from battery; runtime from full charge: ______ h (target ≥ 2 h)
- [ ] Charging while operating works; charge LED states verified
- [ ] Brown-out test: Linux shuts down/recovers without SD corruption (3 cycles)
- [ ] No component exceeds warm-to-touch after 30 min battery operation

## 7. Audio & haptics

- [ ] beep() produces tone at commanded frequency (spot-check by ear/app)
- [ ] Volume 0 is silent; max volume undistorted
- [ ] Vibration motor pulses on command (if fitted)

## 8. Carrier PCB (per PCBWay spin)

- [ ] Visual inspection: no solder bridges, correct component orientation
- [ ] **Before first power:** continuity 5V↔GND open; 3V3↔GND open
- [ ] Rails at spec under load: 5 V = ______, 3V3 = ______
- [ ] Every connector pinout beeped out against the Fusion schematic
- [ ] All I²C devices enumerate through the carrier
- [ ] Buttons register; no ghosting
- [ ] Defects logged → schematic/layout fixes recorded for respin decision

## 9. Enclosure fit (per PCBWay/print iteration)

- [ ] All boards seat on their bosses; no flex on standoff tightening
- [ ] Display aperture aligned; touch usable to panel edges
- [ ] Camera and ToF windows unobstructed (check vignetting in a capture)
- [ ] Buttons actuate through the shell; positive click
- [ ] USB-C, SD accessible assembled; battery secured, wires strain-relieved
- [ ] Assembled weight ______ g; drop-check from 30 cm onto desk (unit survives)

## 10. System integration soak (pre-submission gate)

- [ ] Fully assembled unit: 2-hour continuous run on battery — launcher +
      every app exercised — zero crashes/resets
- [ ] ShadowScan end-to-end: physical object → mesh export → file opens on PC
- [ ] Measurement app vs reference tools: distance ±5 mm, angle ±1°, recorded in docs
- [ ] Cold boot to launcher time: ______ s
- [ ] All contest video/photo shots captured (device in action, in hand)
- [ ] Final BOM reconciled: every physical part appears in BOM.md with real price/source
- [ ] Submission checklist in the contest snapshot doc 100% ticked
