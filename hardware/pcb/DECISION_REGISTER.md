# Perception Head Rev A — unknown / decision register

Every open item, classified. Owner: **M** = Matthew (bench/Fusion),
**A** = agent/software, **M+A** = joint. "Blocks" says what design work waits.
Update the row (don't delete it) when it closes; record the evidence link.

Classes: **RESEARCH** = researchable now · **BENCH** = bench required ·
**MECH** = mechanical required · **SW** = software required · **DECIDE** = decision required.

## Host interface

| ID | Class | Question | Why it matters | Owner | Evidence required | Blocks | Provisional assumption | If wrong |
|---|---|---|---|---|---|---|---|---|
| H1 | BENCH | Is `5V_USB_VBUS` (JANALOG pin 5) live when the UNO Q runs from DC_IN instead of USB-C? | Head 5 V source on battery power | M | DMM on JANALOG 5V with USB-C removed, DC_IN powered; repeat with OTG peripheral attached | Host-carrier power design; ICD power pin | Live on USB-C power (it *is* VBUS); unknown on DC_IN | Head must take 5 V from the host carrier's system rail instead — ICD pin meaning changes, head unchanged |
| H2 | BENCH | 5 V headroom: UNO Q + Media Carrier + DSI display + CSI camera current on a 5 V/3 A source, idle/preview/capture/peak | Sets R15 limit and LED full-scale | M | USB-C inline meter (USBC-VAMETER3) log per POWER_BATTERY_REFERENCE matrix | LED current, R15, battery sizing | ≥0.7 A spare at 5 V | Lower R15/LED current, or move LED power to host-carrier rail |
| H3 | SW | Which Arduino objects map to Qwiic I2C4 (PD12/13) and to D20/D21 I2C2 on the UNO Q Zephyr core (`Wire`/`Wire1`/`Wire2`)? | Firmware for bus A/B | A | Core variant file + a scan sketch on hardware | Host pin freeze | Qwiic = `Wire1`, D20/D21 = `Wire` | Rename in firmware only |
| H4 | BENCH | Pull-ups present on D20/D21 (I2C2) on the UNO Q board? | Bus B pull-up budget with Pololu shifter | M | DMM resistance SDA/SCL→3V3, board unpowered | C18/C19, R3/R4 population | 2.2 kΩ present (schematic reading) | Populate R3/R4 if absent |
| H5 | MECH | With the Media Carrier fitted, are JDIGITAL/JANALOG/Qwiic still reachable for a harness? | Rev A bench harness route | M | Photo of assembled stack | Bench harness | Yes (JMEDIA/JMISC are on the opposite side) | Need a JMISC breakout or host-carrier sooner |
| H6 | DECIDE | Host-side pins for TOF_INT_N / IMU_INT1 / TOF_SYNC / ILLUM_SET | EXTI/timer/DAC conflicts with future encoder/trigger | M+A | Pin plan incl. encoder timer pair; scan of Arduino core EXTI | Schematic freeze (host carrier) | D2 / D7 / D9 / A0(DAC) — see ICD | Re-map harness; head unchanged |
| H7 | MECH | Real cable length/route head→host in Rev A-SM1 | I2C rise time, shifter stability, connector choice | M | Fusion route length + bend radii | Connector family freeze | 150–250 mm | Shorter: fine. Longer: may need I2C buffer on head |
| H8 | DECIDE | Final head connector family (GH / Pico-Lock / FFC) | Production packaging vs bench convenience | M+A | H7 + service concept | Footprint freeze | JST GH 14 for Rev A | Footprint swap only (signal list is stable) |
| H9 | BENCH | I2C4 address map with Media Carrier + any Modulino/Qwiic devices attached | Avoid 0x20/0x50/0x58/0x68 clashes | M | `i2c scan` on Qwiic bus with full stack | Address straps | No clash | Move expander (A pins) or IMU (0x69 via R12) |

## ToF

| ID | Class | Question | Why it matters | Owner | Evidence required | Blocks | Provisional assumption | If wrong |
|---|---|---|---|---|---|---|---|---|
| T1 | BENCH | Does ToF materially improve the Point→Understand workflow? (Lab #3 graduation) | Whether ToF stays on Rev A at all | M | Platypus Lab replayable dataset + comparison | Schematic freeze | Yes, keep provisionally | Delete TOF sheet + bus B + ToF rail; board shrinks |
| T2 | BENCH | Pololu AVDD at VIN = 3.43 V during 8×8 continuous ranging | Rail voltage choice | M | Scope/DMM on carrier AVDD pin under ranging | R5/R6 values | ≥3.2 V | Raise rail (R5) or feed VIN from 5 V with external translation |
| T3 | BENCH | Pololu 1×4 header pin order | Footprint correctness | M | Continuity: GND/SPI-I2C/LP/SYNC to carrier labels | Footprint freeze | GND, SPI/I2C, LP, SYNC from pin row 4 | Footprint edit (5 min) |
| T4 | BENCH | Bus B integrity over the real harness with the Pololu shifter | Pololu warns of oscillation >8 cm | M | Scope SDA/SCL at 400 kHz/1 MHz, 200 mm cable, with/without 22 pF | C18/C19, bus speed | 400 kHz clean, caps DNP | Populate caps, slow bus, or add buffer/shorten cable |
| T5 | BENCH | Back-feed: rail voltage at +3V3_TOF when ToF off and bus B pulled high | Clean 10 ms reset | M | DMM/scope TP6/TP7 with EN low, host pull-ups active | Reset procedure | <0.3 V (active discharge) | Firmware drives bus B low before power-down; or add bus switch |
| T6 | BENCH | Useful ToF mode(s): 4×4@? Hz vs 8×8@15 Hz, continuous vs autonomous, at Platypus working distances | Power, bus load, MCU time | M (Lab) | Lab evidence | Power tree, firmware | 8×8 @ 15 Hz continuous worst case | Lower power/bus load — only helps |
| T7 | SW | ST ULD (~84 KB upload) on UNO Q STM32U585 Zephyr core: memory, I2C 1 MHz support, boot time | Feasibility of MCU-owned ToF | A | Build + run on UNO Q (Pololu on Qwiic bench) | Firmware arch | Fits (2 MB flash) | Run ToF from Linux side (needs 1.8 V-domain bus or different path) |
| T8 | SW | MCU→Linux Bridge throughput/latency for 8×8 frames + IMU stream | Timestamp/sync quality | A | Bridge benchmark on hardware | Sync design | Adequate for 15 Hz frames | Decimate on MCU or send summaries |
| T9 | MECH | ToF window: material, thickness, distance; aperture ≥ exclusion cone 57.9° | Crosstalk <60 cm is the Platypus range | M+A | ST cover-window app note (RESEARCH) + Lab crosstalk cal | Enclosure window | No window for Rev A bench | Crosstalk cal per unit; store in EEPROM record |
| T10 | DECIDE | VL53L8CX (45°) vs VL53L7CX (90°) carrier vs camera FOV 62–75° | Depth coverage of the image | M+A | Lab evidence + camera choice | Footprint (same) | L8CX | Swap carrier, same footprint |
| T11 | RESEARCH | ST AN5897 thermal/PCB guidance and cover-glass guidance documents | Rev B bare IC + window | A | Fetch ST docs (st.com blocked here) | Rev B only | n/a for Rev A | — |

## IMU

| ID | Class | Question | Why it matters | Owner | Evidence required | Blocks | Provisional assumption | If wrong |
|---|---|---|---|---|---|---|---|---|
| I1 | SW | BMI270 driver on UNO Q MCU (Bosch SensorAPI / Arduino lib) with 8 kB config upload | Bring-up | A | Breakout on Qwiic + sketch | Firmware | Works | Different IMU (same bus) |
| I2 | DECIDE | Is IMU used for capture gating (stillness), orientation, or both? Needed ODR/latency | INT routing, sync requirement | M+A | Workflow definition (#37 / Scout) | INT2 routing | Stillness + gravity vector, ≤100 Hz | INT2 to host pin instead of expander |
| I3 | MECH | IMU axis orientation vs camera axes; board stiffness near IMU | Calibration meaning | M | Fusion placement | Layout | Axes aligned to camera, documented | Rotation stored in calibration record |

## Illumination

| ID | Class | Question | Why it matters | Owner | Evidence required | Blocks | Provisional assumption | If wrong |
|---|---|---|---|---|---|---|---|---|
| L1 | BENCH | Does controlled illumination improve measurement repeatability under the STATUS.md lighting conditions? | Whether illumination earns its place | M | Captures with/without a bench LED at fixed current, same scenes | Whether sheet stays | Yes, modestly | Remove sheet; keep connector footprints DNP |
| L2 | BENCH | Required LED current/flux at working distance (camera exposure/gain tolerable) | LED part + full-scale | M | Exposure/gain logs vs LED current using a bench CC supply | R16/R17, R22/R23, LED choice | ≤250 mA/ch | Re-scale divider/Rsense; maybe higher-power LED + MCPCB |
| L3 | BENCH | Glare/specular hotspots vs LED offset/angle; diffuser need | Light-board geometry | M | Shiny-part captures at 2–3 LED positions | Light-board design, Fusion | Two LEDs ±25–35 mm off axis, diffused | Ring light or cross-polarisation (future) |
| L4 | BENCH | BJT and LED temperature at chosen current/duty | Copper area, duty limits | M | Thermocouple/IR after 5 min on | Layout copper | ≤60 °C rise at 250 mA continuous | Pulse-only (flash) operation |
| L5 | BENCH | PWM banding with the selected IMX219 module if PWM-dim is used | Justifies analog set-point | M | Capture at 1/5/25 kHz PWM vs DC | ILLUM_SET source | Banding visible → DAC/filtered | n/a (design already avoids it) |
| L6 | DECIDE | LED part (CRI ≥90 mid-power, e.g. 3030/3535 class) | Colour fidelity, flux | M+A | L2/L3 results + availability | Light board | High-CRI 3030-class | Different footprint on light board only |
| L7 | SW | STM32 DAC output on A0 via Arduino core (or PWM fallback) | ILLUM_SET source | A | Sketch on hardware | Host pin freeze | DAC usable | Use PWM ≥20 kHz into existing RC filter |

## Power

| ID | Class | Question | Why it matters | Owner | Evidence required | Blocks | Provisional assumption | If wrong |
|---|---|---|---|---|---|---|---|---|
| P1 | BENCH | Measured system matrix (UNO Q, +Media Carrier, +display, +camera, +ToF, +LEDs) | Battery + host carrier | M | See POWER_TREE_REV_A.md §Bench | Battery sizing, host carrier | none (UNKNOWN) | — |
| P2 | BENCH | Pololu carrier VIN current: idle, ranging 4×4/8×8, peak | LDO thermal, rail sizing | M | SH2 voltage or bench meter | Power tree | 100 typ / 150 peak mA (vendor) | Bigger LDO package if >300 mA |
| P3 | BENCH | Head inrush on hot-plug (C2+C4+C6+C7+C8 ≈ 20 µF) | Host rail disturbance | M | Scope host 5 V/3V3 during plug | Connector/hot-plug policy | Negligible | Add soft-start or "no hot-plug" rule |

## Calibration / sync

| ID | Class | Question | Why it matters | Owner | Evidence required | Blocks | Provisional assumption | If wrong |
|---|---|---|---|---|---|---|---|---|
| C1 | SW | EEPROM record schema + host calibration file format | Identity/provenance | A | Schema doc + reader/writer with tests | Firmware | Versioned binary + CRC, ≤512 B | — |
| C2 | DECIDE | Acceptable RGB↔ToF timestamp error for Rev A (handheld still capture vs motion) | Whether TOF_SYNC needed | M+A | Use-case definition; IMU motion stats | Keep/drop TOF_SYNC | Still capture; ≤50 ms OK; SYNC kept as cheap option | Drop the pin |
| C3 | BENCH | Extrinsic repeatability after head remove/refit (datum holes) | Proves Rev A goal #3 | M | 5× remove/refit, re-capture fixed target | Datum design | <0.2 mm / <0.2° class | Add dowels / change locating scheme |
| C4 | RESEARCH | AT24CS32 serial-number read sequence & page size | Firmware | A | DS20006087 | Firmware | Security address 0x58 | Trivial firmware change |

## Camera / mechanics

| ID | Class | Question | Why it matters | Owner | Evidence required | Blocks | Provisional assumption | If wrong |
|---|---|---|---|---|---|---|---|---|
| M1 | BENCH | Selected reference and product camera (#40) | Mount holes, optical centre, clearance | M | [Camera intake table](CAMERA_EVIDENCE_INTAKE.md) | Layout | B0394 reference | Hole pattern change (H4–H7 positions only) |
| M2 | BENCH | Camera board hole pattern / lens stack height / FFC exit (measured) | Head outline | M | Calipers + photo per module (T5) | Layout | Pi-camera 21 × 12.5 mm pattern | Move H4–H7 |
| M3 | MECH | Head outline envelope, datum-hole positions, connector exits from Fusion | Layout start | M | Fusion export (DXF/STEP) | Layout | ≤60 × 35 mm | — |
| M4 | DECIDE | Does the head PCB itself carry the camera (current plan) or does a machined bracket carry both? | Datum ownership | M+A | Fusion study + C3 | Layout | Head PCB carries both; bracket locates head | Head becomes ToF/IMU/LED only; extrinsic owned by bracket |
| M5 | DECIDE | Board thickness 1.6 mm vs 1.0 mm | Stiffness at datum | M+A | Fusion stack-up | Fab | 1.6 mm FR-4 | — |
