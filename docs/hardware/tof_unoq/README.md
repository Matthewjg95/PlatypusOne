# Pololu VL53L8CX (#3419) directly on the Arduino UNO Q — bring-up packet

Issues: #16 (ToF bring-up, 2026-10-16 path change) · #33 (Rev A perception carrier).
Status: **prepared, not physically validated.** No wire has been soldered, no rail measured
and no frame captured as part of this work. The operator closes every physical gate.

| Document | Purpose |
|---|---|
| this README | verdict, chosen connection, exact pin table, parts, software, risks |
| [SOURCES.md](SOURCES.md) | every fact with its document, version and hash; derived reasoning |
| [BENCH_CHECKLIST.md](BENCH_CHECKLIST.md) | Gates A–F with stop/go criteria, troubleshooting tree, evidence templates |
| [diagrams/](diagrams/) | system, soldering, connection and inspection diagrams (SVG + PNG previews) |
| [firmware/tof_unoq_vl53l8cx](../../../firmware/tof_unoq_vl53l8cx/README.md) | MCU sketch: I²C scan → ID → ST ULD init → 8×8 frames |
| [tools/tof_unoq](../../../tools/tof_unoq/README.md) | Linux logger (CSV), analysis, camera+ToF co-acquisition, tests |

Earlier work this builds on, not repeated here: PR #21 (Tab5/M024 procedure and gate structure),
PR #44 (`hardware/pcb/RESEARCH_LEDGER.md`, ICD §5a domain analysis, Pololu footprint),
PR #42/#48 (CSI camera path).

## 1. Verdict

**The documentation supports the direct connection.** Pololu #3419 on the UNO Q Qwiic
connector, VIN from the Qwiic 3.3 V, I²C on STM32 I2C4 (`Wire1`), SPI/I2C strapped to GND.
Every net is consistent with the manufacturers' documents. Two margins are **not
guaranteed by any document** and are measured at Gate C:

1. **AVDD headroom.** The carrier's AVDD = VIN minus an unpublished LDO dropout. At
   VIN = 3.3 V the inferred AVDD is about 3.2 V, against a 3.13 V sensor minimum. The UNO Q
   rail's own specification minimum (3.1 V) is below the carrier's 3.2 V VIN minimum.
2. **3.3 V load.** Up to 150 mA extra on a 2 A buck that also feeds the MCU, ANX7625 and
   Wi-Fi. Arduino documents no external allowance.

The earlier five-wire hypothesis (issue #16) is correct with one refinement. The fifth
connection, SPI/I2C → GND, is made **on the carrier** with a shunt to its adjacent GND pin,
not as a wire to the UNO Q. So: four wires to the UNO Q plus one local jumper.

## 2. Connection options compared

| | A. Qwiic → female-socket cable (**recommended**) | B. Arduino headers (`Wire` = I2C2, D20/D21) | C. 5 V VIN + I²C level translator |
|---|---|---|---|
| Bus | I2C4 PD13/PD12, `Wire1` | I2C2 PB11/PB10, `Wire` (sketch edit) | either |
| Power | Qwiic pin 2 = PWR_3P3V | header +3V3 (same PWR_3P3V rail) | 5 V pin → VIN (AVDD headroom solved) |
| Soldering | headers on the Pololu only | headers on the Pololu only | headers + translator breakout |
| Mis-wiring risk | UNO Q end **keyed**; Pololu end four sockets | four loose Dupont ends at **two** UNO Q headers, unkeyed | most connections |
| Pull-ups | 2.2 kΩ on UNO Q (DS) | 2.2 kΩ shown on schematic, **BENCH_VERIFY (H4)** | translator-dependent |
| Lead length | 150 mm cable (Pololu advises < 80 mm) | similar or longer | longer |
| Other users of the pins | JMISC PF14/15 shares the peripheral but not the pins | PB10/PB11 also TIM2 PWM (core note) | — |
| Rev A relevance | same bus/connector family as the bench harness | Rev A bus B option | only if Gate C fails |
| Verdict | **use** | fallback if Qwiic is physically blocked | **only if Gate C fails** (AVDD < 3.13 V or VIN < 3.2 V under load); needs its own review |

No custom PCB is needed for A or B.

## 3. Exact connections (option A)

Board orientation: **Pololu top view = sensor side**, J1 (1×9) on the left edge, J2 (1×4)
and the mounting holes on the right. J1 pin 1 (CORE/IOVDD) is at the top. The **square
pads are the two GND pins** (J1-4, J2-1), not pin 1. Pin numbering is Pololu's schematic
connector numbering. See [the soldering diagram](diagrams/02_pololu_soldering.svg).

| # | UNO Q end | Net / level | Wire colour* | Pololu end | Source |
|---|---|---|---|---|---|
| 1 | QWIIC1 pin 1 GND | 0 V | black | **J1-4 GND** (square pad) | A1 §9.3, P1, P2 |
| 2 | QWIIC1 pin 2 +3V3 | PWR_3P3V, 3.1–3.5 V spec | red | **J1-3 VIN** (3.2–5.5 V) | A1 §9.3, A2 s.20, P1 |
| 3 | QWIIC1 pin 3 SDA | PD13 I2C4_SDA, 3.3 V, 2.2 kΩ pull-up | blue | **J1-5 SDA/MOSI** | A1 §9.3, A2 s.20, P2 |
| 4 | QWIIC1 pin 4 SCL | PD12 I2C4_SCL, 3.3 V, 2.2 kΩ pull-up | yellow | **J1-6 SCL/MCLK** | A1 §9.3, A2 s.20, P2 |
| 5 | — (on carrier) | SPI/I2C held at 0 V → I²C mode | 2-pin shunt | **J2-2 SPI/I2C ↔ J2-1 GND** | P1 |
| — | not connected | regulator **outputs** | — | J1-1 CORE/IOVDD, J1-2 AVDD | P1 |
| — | not connected | unused in I²C polling | — | J1-7 MISO, J1-8 CS, J1-9 INT, J2-3 LP (pulled high = I²C on), J2-4 SYNC | P1 |

\* Qwiic colour convention (SparkFun Qwiic page; Adafruit #4397). A different cable may use
other colours. **Verify each conductor by continuity at Gate B.** Never trust colour alone.

**Highest-consequence error:** the red wire one pin up lands on **J1-2 AVDD** (an LDO
output, sensor absolute maximum 3.47 V). Two pins up lands on **J1-1 CORE/IOVDD** (1.8 V
rail, absolute maximum 1.98 V). Either feeds the UNO Q 3.3 V into a regulator output.
Gate B item B8 and the inspection diagram target this.

[Where each wire terminates](diagrams/03_unoq_qwiic_connection.svg) ·
[full system](diagrams/01_system_wiring.svg) · [pre-power inspection points](diagrams/04_preinspection_points.svg)

## 4. Parts needed

| Qty | Part | Note |
|---|---|---|
| 1 | Pololu #3419 VL53L8CX carrier | in hand per project records; identity checked at Gate A |
| 1 | 1×9 straight 0.1″ male header | **not included** with the carrier |
| 1 | 1×4 straight 0.1″ male header | **not included** |
| 1 | 2-pin 0.1″ shunt (jumper) | GND ↔ SPI/I2C; Pololu's suggested method |
| 1 | Qwiic/STEMMA QT 4-pin to **female** 0.1″ sockets cable, e.g. Adafruit #4397 (150 mm) | the shortest available; 150 mm is above Pololu's < 80 mm advice (risk R3) |
| — | multimeter; scope optional but recommended for Gate D | |
| 2 | M2 screws + standoffs (optional) | hold the carrier still for ranging (holes Ø2.18 mm) |

The BOM line 20 ("JST-SH/Qwiic cables, headers, M2 standoffs kit") already plans these.

## 5. Software

**Execution architecture.** The Qwiic I²C bus belongs to the **STM32U585 MCU**. The Linux
side (QRB2210) has no electrical access to it. The sensor driver therefore runs on the MCU:

```
VL53L8CX ── I2C4 400 kHz ── STM32U585 sketch (ST ULD 1.3.0, bounded I/O)
                                │ Monitor (Bridge RPC over UART, 115200 baud)
                                ▼
                 arduino-router ── TCP 127.0.0.1:7500 ── tof_log.py  →  CSV + JSON
                                                         coacq.py   →  + camera frames, pairs.csv
```

* Sketch `firmware/tof_unoq_vl53l8cx`: waits for a logger, then runs and reports BUS_BEGIN,
  SCAN, ACK, **ID check (0xF0/0x0C)**, INIT (84 KB firmware upload), CFG (read back from the
  sensor), RANGING_START, then FRM records. Each record is a CRC-checked text line. An ACK
  is reported as "does not prove ranging". Only FRM records count.
  Compiled for `arduino:zephyr:unoq` (core 1.0.0): 195,856 B flash, 51,852 B RAM.
  **Not yet run on hardware.**
* `tools/tof_unoq/tof_log.py` (on the UNO Q): timestamped raw log, frames.csv, zones.csv
  (distance, ST target status and confidence class, sigma per zone), session.json
  (configuration, error diagnosis, CRC rejects, sequence gaps).
* `tools/tof_unoq/analyze.py`: frame timing, MCU→host latency envelope, per-zone and
  central-zone statistics, raw median versus operator reference. **No correction and no
  pass/fail.**
* `tools/tof_unoq/coacq.py`: camera and ToF acquired independently, paired by Linux
  CLOCK_MONOTONIC; V4L2 buffer timestamps when `v4l2-ctl --verbose` reports
  `ts-monotonic`. **No hardware synchronisation.**

Build, flash, run and recovery commands are in the
[firmware README](../../../firmware/tof_unoq_vl53l8cx/README.md) and the
[tools README](../../../tools/tof_unoq/README.md).

## 6. Risks and open items

| ID | Risk | Evidence state | Mitigation / gate |
|---|---|---|---|
| R1 | AVDD below 3.13 V under 8×8 ranging at VIN ≈ 3.3 V | inferred margin ≈ 70 mV; LDO dropout unpublished | Gate C V2 while ranging; fallback C |
| R2 | PWR_3P3V load / droop (shared 2 A buck, no documented allowance) | unknown | Gate C: VIN under load, UNO Q stability, 5 V input delta |
| R3 | NXS0108 shifter oscillation with a 150 mm cable | Pololu warning | Gate D: scope SDA/SCL if available; shorten the cable; tens of pF if needed (decision register T4) |
| R4 | Zephyr Wire sends STOP between index write and read (no repeated start) | DS14161 permits stop-terminated messages; the ID check confirms on the bench | Gate D ID_OK |
| R5 | The sensor can only be reset by removing its supply; Qwiic 3.3 V is not switchable | DS (UM3109 §4.2) + A2 | recovery = power the whole board off; Rev A already plans a switchable rail |
| R6 | Monitor/Bridge bandwidth (115200 baud) limits text frames | derived ≈ 0.8 KB per 8×8 frame; 5 Hz default | Gate E checks sequence gaps; decision register T8 |
| R7 | Camera driver path is unmerged (PR #48/#42); CSI port mapping unresolved | REPO | Gate F uses whatever camera path is working, records it |
| R8 | Board identity and revision not photographed in the repo | UNRESOLVED | Gate A1 photo of both sides and silkscreen |
| R9 | Cable colours differ from the convention | possible | Gate B continuity, never colour |
| R10 | Header-mounted pose and heat: carrier on 13 pins; offset drift ~0.1 mm/°C (DS) | DS | mount with M2; record silicon temperature (in every FRM) |

## 7. Implications for Rev A (issue #33)

Recorded so the PCBWay build is designed from evidence. **Nothing is routed or ordered.**

* `Wire` = I2C2 (D20/D21), `Wire1` = I2C4 Qwiic: decision register H3 is answered by the
  core source (bench scan still confirms).
* Pololu J2 order GND, SPI/I2C, LP, SYNC, aligned with J1 pins 4–7: **document-verified**
  (Pololu schematic + photos). T3 still needs a physical continuity check. The PR #44
  footprint matches. Note: the footprint squares pad 1 (CORE/IOVDD), while the real
  carrier squares its GND pads. Label the silkscreen to avoid assembler confusion.
* The Qwiic 3.3 V cannot be switched, so it cannot perform the 10 ms supply-cycle reset.
  This supports Rev A's switchable ToF rail.
* Gate C at VIN = PWR_3P3V directly measures T2 (AVDD at VIN ≈ 3.3 V). Its result decides
  whether Rev A's 3.29 V rail is enough.
* Bridge throughput (T8) constrains 8×8 frame rate and timestamp quality. Gate E measures
  the arrival jitter.
