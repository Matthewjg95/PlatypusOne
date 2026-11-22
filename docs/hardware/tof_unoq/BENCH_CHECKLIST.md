# Bench checklist — Pololu VL53L8CX direct on the UNO Q

**Only the human operator closes a gate.** Each gate lists what to do, what to record, and
the stop/go rule. A STOP means: power off, record what was seen, and do not continue until
the cause is understood. Nothing in this repository has passed any of these gates yet.

Reference tables: [README §3 pin table](README.md#3-exact-connections-option-a) ·
[inspection diagram](diagrams/04_preinspection_points.svg) · [SOURCES](SOURCES.md).

Create one evidence folder per session, e.g. `~/tof-evidence/20261016-unoq-pololu/`, and
fill the [templates](#evidence-templates) as you go.

---

## Gate A — Documentation (operator confirms the desk work)

| # | Check | Record | Go |
|---|---|---|---|
| A1 | Photograph the carrier **both sides**, in focus. Read the bottom silkscreen: "Pololu", "irs19b", the J/number code, "SPI/I2C", "LP", "SYNC" labels | photos + transcribed markings | markings match Pololu photo 0J12379 (#3419) |
| A2 | Count pins and pads: 1×9 + 1×4; J2 next to J1 pins 4–7; two square GND pads | photo with the labels visible | matches the soldering diagram |
| A3 | Confirm the cable: 4-pin JST-SH (Qwiic/STEMMA QT) to **female** 0.1″ sockets; note length and brand | photo + length | length recorded (shorter is better; Pololu < 80 mm ideal) |
| A4 | Read [README §1–3](README.md) and the two unguaranteed margins (AVDD headroom, 3.3 V load) | initials | operator agrees to Gate C limits |
| A5 | Firmware compiles on the operator's machine: `arduino-cli compile --profile unoq firmware/tof_unoq_vl53l8cx` | compiler output | "Sketch uses … bytes"; same numbers as the README ± a little |

**STOP** if the board markings do not match #3419, or the pin labels differ from the diagram.
Do not wire a board you cannot identify.

## Gate B — Unpowered inspection (everything disconnected from power)

Power state: UNO Q USB-C **and** DC_IN unplugged. Wait 30 s. Confirm 0 V on the UNO Q 3.3 V
pin before using ohms. Assemble per the [soldering diagram](diagrams/02_pololu_soldering.svg):
headers soldered on the sensor side with the bodies underneath, shunt on J2-1/J2-2, and the
cable sockets on J1-3, J1-4, J1-5, J1-6. Plug the Qwiic end into the UNO Q (keyed).

| # | Measure (meter in ohms/continuity) | Expect | Record |
|---|---|---|---|
| B0 | Visual under magnification: every joint wetted, no bridges, no flux on the sensor window; liner still on | clean | photo of both sides |
| B1 | UNO Q-side GND (e.g. JSPI pin 6 or a GND header pin) ↔ Pololu J1-4 | continuity | Ω |
| B2 | UNO Q +3V3 header pin ↔ Pololu J1-3 VIN | continuity (same PWR_3P3V net through the cable) | Ω |
| B3 | Black/red/blue/yellow: at the socket ends, check each lands on J1-4/J1-3/J1-5/J1-6 respectively | correct mapping | table |
| B4 | J2-2 SPI/I2C ↔ J2-1 GND | ≈ 0 Ω (shunt) | Ω |
| B5 | VIN–GND, SDA–GND, SCL–GND, SDA–SCL | **not** a short; values settle as capacitors charge | Ω after 5 s |
| B6 | Each adjacent pair on J1 (1-2, 2-3, … 8-9) and on J2 | no bridge (no ≈0 Ω except the J2-1/J2-2 shunt) | list |
| B7 | **J1-3 VIN ↔ J1-2 AVDD and J1-3 VIN ↔ J1-1 CORE/IOVDD** | not ≈0 Ω: separate nets | Ω |
| B8 | Red socket is on J1-3 (VIN), **not** J1-2 or J1-1 | visual + B3 | photo |

Readings through ICs and capacitors vary by meter and polarity. Record values. Do not turn
an ambiguous reading into a pass. **STOP** on any unexpected ≈0 Ω path, any mis-mapped
conductor, or power-to-regulator-output mis-wiring.

## Gate C — Power (operator-measured rails)

Use a USB-C power meter between the 5 V/3 A supply and the UNO Q if available. Measure with
a DMM at the Pololu pin tops (top side), with a hooked or insulated probe. **Never** bridge
adjacent pins with a bare tip.

Do **not** put a DMM in series with VIN on a mA range. Its shunt drops voltage (often
0.1–1 V at 150 mA) and starves the carrier, which falsifies the very headroom being
measured. Measure current on the 5 V input with the USB meter instead.

| # | State | Measure | Go | STOP |
|---|---|---|---|---|
| C0 | UNO Q powered, **sketch not flashed or logger not started** (sensor idle) | 5 V input current baseline (USB meter) | recorded | — |
| C1 | same | V1 VIN–GND at J1-3/J1-4 | 3.20–3.50 V | < 3.20 V or > 3.50 V |
| C2 | same | V2 AVDD–GND at J1-2 | ≥ 3.13 V and ≤ 3.47 V | outside |
| C3 | same | V3 CORE/IOVDD–GND at J1-1 | 1.62–1.98 V | outside |
| C4 | same | V4 SDA, V5 SCL to GND | ≈ VIN (pulled up) | ≈ 0 V (stuck low) or > VIN + 0.1 V |
| C5 | same | V6 SPI/I2C–GND ≈ 0 V; V7 LP–GND ≈ VIN | as stated | SPI/I2C high (shunt missing) |
| C6 | touch test after 2 min | carrier and UNO Q not hot to the touch; no smell | normal | any heating |
| C7 | **after Gate D/E start ranging 8×8 continuous** | repeat V1 and V2 during ranging, several readings over 1 min; 5 V input current delta vs C0 | V1 ≥ 3.20 V, **V2 ≥ 3.13 V** throughout | either below its limit → fallback C (README §2) |

A DMM shows an average. A scope on AVDD during INIT and ranging (optional) shows dips the
DMM cannot. If a reading is within meter uncertainty of a limit, record it as **not
passed**.

## Gate D — Communication (sensor detected *and* identified)

On the UNO Q (Linux shell):

```sh
systemctl status arduino-router            # must be active
python3 tools/tof_unoq/tof_log.py --out ~/tof-runs/D-$(date +%Y%m%dT%H%M%S) \
    --frames 5 --label gateD --operator "<name>"
```

| # | Logger shows | Go | STOP / next step |
|---|---|---|---|
| D1 | `HELLO` with sketch 0.1.0 and uld VL53L8CX_1.3.0 | yes | no HELLO in 30 s → tree T1 |
| D2 | `SCAN count=1 addrs=0x29` | exactly 0x29 | other addresses → record; none → tree T2 |
| D3 | `ACK addr7=41` (0x29) | yes | `NO_ACK` → tree T2 |
| D4 | `ID_OK device_id=240 revision_id=12` (0xF0/0x0C) | yes | `ID_FAIL` → tree T3 |
| D5 | `INIT_OK ms=…` | yes, ms recorded | `INIT_FAIL` → tree T4 |
| D6 | `CFG zones=64 hz=5 …` read back from the sensor | recorded | `CFG_FAIL` → tree T5 |

An ACK alone does not pass Gate D. D4 and D5 are required. Save `raw.log` and `session.json`.
Optional: scope SDA/SCL during INIT (rise time, ringing, high level ≈ VIN).

## Gate E — Ranging at known distances

Setup: carrier fixed (M2 screws or clamp), **liner removed**, aimed at a flat matte target
large enough to fill the 45° × 45° field of view at that distance (≈ 0.83 × distance per
side), normal to the axis. Measure the distance from the sensor's front face to the target
with a tape or ruler and record its uncertainty. Record lighting.

For each distance (suggested 100, 300, 500, 1000 mm; then repeat 500 mm after re-mounting):

```sh
python3 tools/tof_unoq/tof_log.py --out ~/tof-runs/E-500mm-a --frames 100 \
    --label 500mm-white-card --ref-distance-mm 500 --target "white card 600x600 mm, matte" \
    --operator "<name>"
python3 tools/tof_unoq/analyze.py ~/tof-runs/E-500mm-a
```

| # | Criterion | Record | Go |
|---|---|---|---|
| E1 | ≥ 100 frames, `crc_or_format_rejects` and `sequence_gaps` counted | numbers | frames arrive continuously; gaps explained |
| E2 | Central zones mostly status 5 on the full-FoV target | valid fraction grid | recorded (no threshold invented here) |
| E3 | Central median vs operator reference | `median_minus_reference_mm` | **operator judges** against DS14161 Table 18 conditions; no correction is derived |
| E4 | Repeatability | central per-frame stdev; repeat run after re-mounting | recorded |
| E5 | Rails during ranging = Gate C7 | V1/V2 | as Gate C |
| E6 | 10-minute run at 5 Hz 8×8 | `DATA_TIMEOUT`/`READ_FAIL` counts, temperature trend | no stall |

Do not tune anything from one session. Platypus Lab issue #3 owns the wider
characterization; this gate only shows the direct UNO Q path produces real depth frames.

## Gate F — Camera + ToF integration

Precondition: a camera path that works **on its own** (PR #48/#42 notes), and Gate E passed.

```sh
# start with the ToF idle-tested, then:
python3 tools/tof_unoq/coacq.py --out ~/tof-runs/F-coacq-01 --camera-frames 20 --interval 1 \
    --camera-cmd 'tools/camera_characterize/capture_raw.sh {out}' --label coacq-500mm \
    --ref-distance-mm 500 --operator "<name>"
```

| # | Criterion | Go |
|---|---|---|
| F1 | All camera captures return 0 and write files while ToF ranges | `failed_commands` = 0 |
| F2 | ToF continues: no `DATA_TIMEOUT`, `READ_FAIL`, resets or sequence gaps beyond the Gate E baseline | compare to E |
| F3 | Display stays lit and touch responds during the run (if fitted) | operator observation |
| F4 | `dmesg` shows no new camss/I2C/USB/power errors during the run | save `dmesg` before/after |
| F5 | 5 V input current with camera + display + ToF recorded (decision register H2/P1) | numbers |
| F6 | V1/V2 still within Gate C limits with everything running | numbers |
| F7 | `pairs.csv`: every camera frame has a ToF neighbour; |dt| recorded; manifest says "independent acquisition" | recorded; no sync claim |

---

## Troubleshooting decision tree

```
T1 No HELLO within 30 s of starting tof_log.py
 ├─ "could not connect" → arduino-router not running: sudo systemctl restart arduino-router
 ├─ connected, nothing arrives →
 │    ├─ another monitor client (App Lab serial monitor) connected? close it, retry
 │    ├─ sketch not flashed / old sketch → re-upload (firmware README)
 │    └─ still nothing → re-upload a known-good Bridge example (Arduino_RouterBridge "monitor");
 │       if that also prints nothing, the problem is the MCU/Bridge, not the ToF
 └─ HELLO arrives but the logger was started late → it sends STATUS; bring-up events are in
    an earlier session only. Power-cycle and start the logger first for a full record.

T2 SCAN count=0 / NO_ACK
 ├─ POWER OFF first. Then:
 ├─ Gate B again: B1–B4 (cable mapping), B8 (red on VIN)
 ├─ SPI/I2C shunt present? (C5: SPI/I2C must read ≈0 V when powered) — default is SPI mode
 ├─ VIN present at the carrier? (C1)
 ├─ SDA/SCL swapped? (blue→J1-5, yellow→J1-6)
 └─ still none → scope SDA/SCL: no activity = MCU side; activity but no ACK = sensor/shifter

T3 ACK but ID_FAIL
 ├─ another device at 0x29? (SCAN list shows extra addresses → remove other Qwiic devices)
 ├─ marginal bus: scope rise time/ringing; shorten cable; (T4 decision-register item: tens of pF)
 └─ bytes read are consistent but wrong → wrong part: compare Gate A photos with 0J12379

T4 INIT_FAIL (uld_status in the event; 255 = comms error per UM3109 Table 5)
 ├─ i2c_failures > 0 → bus integrity during the 84 KB upload: shorten cable, scope, check rails
 ├─ AVDD/VIN dip during INIT? (scope, or repeat C1/C2 while INIT runs)
 ├─ power-cycle the WHOLE board (sensor reset = supply removal, UM3109 §4.2), retry once
 └─ repeatable failure after a clean power cycle → stop; capture raw.log + rails, report on #16

T5 CFG_FAIL uld_status=127 → invalid setting (e.g. HZ > 15 at 8×8); fix the command and retry

T6 Frames arrive but are odd
 ├─ all zones status 255 / no target → liner still on? target too far/too dark? aimed at sky?
 ├─ many status ≠ 5 in central zones on a full-FoV target → lighting, target, tilt; record, do not tune
 ├─ sequence_gaps grow → Monitor/Bridge throughput: lower HZ (--hz 2) or RES 4; record (T8)
 └─ DATA_TIMEOUT / RANGING_STALLED → rails under load (Gate C7), temperature, cable

T7 UNO Q resets or Wi-Fi drops when ranging starts
 └─ 3.3 V load/droop: STOP. Record 5 V current and V1. Do not continue on this power path;
    discuss fallback C or a separate 3.3 V supply (needs its own review) on #16.
```

---

## Evidence templates

### Session header (`session-notes.md`)

```markdown
# ToF direct UNO Q session <YYYYMMDD-HHMM>
Operator: <name>      Location/lighting: <...>
UNO Q: serial <...>, Debian image <uname -a>, arduino-router <dpkg -s arduino-router | Version>
Media Carrier fitted: yes/no   Camera: <model, connector>   Display: <model>
Carrier: Pololu #3419, markings "<bottom silkscreen text>"  photos: A1-top.jpg, A1-bottom.jpg
Cable: <brand/part, length mm>   Headers: <type>   Shunt: yes
Sketch: tof_unoq_vl53l8cx <git SHA of this repo>, compiled with arduino:zephyr@1.0.0
Supply: <USB-C PSU model, rating>  Meter: <DMM model>  Scope: <model or none>
```

### Gate B record

| Item | Reading | Pass/Fail/Note (operator) |
|---|---|---|
| B0 visual | photo names | |
| B1 GND continuity | Ω | |
| B2 3V3→VIN continuity | Ω | |
| B3 colour→pin map | black→J1-_, red→J1-_, blue→J1-_, yellow→J1-_ | |
| B4 shunt | Ω | |
| B5 VIN–GND / SDA–GND / SCL–GND / SDA–SCL | Ω / Ω / Ω / Ω (after 5 s) | |
| B6 adjacent pairs | list any < 10 Ω | |
| B7 VIN–AVDD / VIN–CORE | Ω / Ω | |
| B8 red on VIN | photo | |

### Gate C record

| Item | Idle | Ranging 8×8 (C7) | Limit | Operator decision |
|---|---|---|---|---|
| 5 V input current (USB meter) | A | A (Δ = ) | — | |
| V1 VIN | V | V min / V typ | 3.20–3.50 V | |
| V2 AVDD | V | V min / V typ | 3.13–3.47 V | |
| V3 CORE/IOVDD | V | V | 1.62–1.98 V | |
| V4/V5 SDA/SCL idle | V / V | — | ≈ VIN | |
| V6/V7 SPI/I2C / LP | V / V | — | ≈0 / ≈VIN | |
| heating after 2 min / 10 min | | | none | |

### Gate E record (one row per run)

| Run dir | Ref distance (mm) ± | Target | Frames | Rejects/gaps | Central median (mm) | median − ref (mm) | Central stdev (mm) | Silicon temp (°C) | Notes |
|---|---|---|---|---|---|---|---|---|---|
| | | | | | | | | | |

### Gate F record

| Run dir | Camera cmd | Camera frames ok/requested | ToF frames | ToF gaps vs E | |dt| median/max (ms) | Display/touch OK | dmesg new errors | 5 V current (A) | V1/V2 (V) |
|---|---|---|---|---|---|---|---|---|---|
| | | | | | | | | | |

Raw files to keep for every run: the whole `tof_log`/`coacq` output directory (raw.log,
CSVs, session.json, analysis.*), photos, `dmesg` before/after, and session-notes.md.
