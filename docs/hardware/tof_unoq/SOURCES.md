# Sources and fact ledger — UNO Q + Pololu VL53L8CX direct bring-up

Retrieved 2026-10-16. Documents were downloaded and hashed so a later reader can tell if
a vendor revised them. **Nothing in this file is a bench measurement.**

Tags: **DS** manufacturer document · **CODE** vendor source code at the stated commit ·
**DERIVED** our inference from DS/CODE facts (reasoning shown) · **UNRESOLVED** must be
checked physically or by the operator.

## Documents

| ID | Document | Version / date | Where | SHA-256 (first 16) |
|---|---|---|---|---|
| A1 | Arduino UNO Q (ABX00162) datasheet | "Modified 17/06/2026" | docs.arduino.cc/resources/datasheets/ABX00162-datasheet.pdf | `f87089a39346cd11` |
| A2 | UNO Q schematics | 25 sheets, sheet date 01/10/2025 | docs.arduino.cc/resources/schematics/ABX00162-schematics.pdf | `59a22a0e966fc8b2` |
| A3 | UNO Q full pinout | "Last update 17 Feb 2026" | docs.arduino.cc/resources/pinouts/ABX00162-full-pinout.pdf | `c4cd57bf718939eb` |
| A4 | UNO Media Carrier (ASX00083) datasheet | "Modified 15/10/2026" | docs.arduino.cc/resources/datasheets/ASX00083-datasheet.pdf | `fab8bad71abbb121` |
| P1 | Pololu #3419 product page + photos 0J12377 (top), 0J12378 (breadboard), 0J12379 (bottom, dimensioned), 0J12380 (pinout) | page retrieved 2026-10-16 | pololu.com/product/3419 | 0J12380 `06057e760dde815f`, 0J12379 `69a81e24285ce9d4`, 0J12378 `9d13cc7b025d47c1`, 0J12377 `0f37decc319a6ae4` |
| P2 | Pololu #3419 schematic | 1 page | pololu.com/file/0J2031 | `57047cd4a6fa93d4` |
| S1 | ST VL53L8CX datasheet DS14161 | Rev 7, Aug 2023 | Pololu mirror pololu.com/file/0J2029 | `007cae46440445ea` |
| S2 | ST UM3109 (ULD guide) | Rev 7, Apr 2024 | Pololu mirror pololu.com/file/0J2030 | `1649d77e38ebb659` |
| T1 | TI TPS62A02A product page | retrieved 2026-10-16 | ti.com/product/TPS62A02A | — |
| Q1 | SparkFun Qwiic connect system page | retrieved 2026-10-16 | sparkfun.com/qwiic | — |
| Q2 | Adafruit #4397 STEMMA QT/Qwiic to female sockets, 150 mm | retrieved 2026-10-16 | adafruit.com/product/4397 | — |

## Source code (read at these commits)

| ID | Repository | Commit / release | What was read |
|---|---|---|---|
| C1 | arduino/ArduinoCore-zephyr | `d0cb476` (2026-10-15); installed release `arduino:zephyr@1.0.0` matches | `variants/arduino_uno_q_stm32u585xx/*.overlay`, `.conf`; `libraries/Wire/Wire.{h,cpp}`; `platform.txt`, `boards.txt` |
| C2 | stm32duino/VL53L8CX | `a93a9d6` = library 2.1.0, ULD API `VL53L8CX_1.3.0` | `vl53l8cx_api.{c,h}`, `vl53l8cx_platform.{c,h}`, `vl53l8cx.h` (C++ I/O) |
| C3 | arduino-libraries/Arduino_RouterBridge | `b8ec5b4` = 0.4.3 | `README.md`, `src/monitor.h`, `src/bridge.h`, `examples/monitor` |
| C4 | arduino/arduino-router | `cb7b2ce` | `main.go` (flags), `internal/monitorapi/monitor-api.go` |
| C5 | v4l-utils (linuxtv git, master, 2026-10-16) | — | `utils/v4l2-ctl/v4l2-ctl-streaming.cpp` `print_concise_buffer()`, `utils/common/v4l2-info.cpp` `bufferflags2s()` |
| R1 | this repo, `hardware/pcb/RESEARCH_LEDGER.md`, `ICD_PERCEPTION_HEAD.md`, footprint `Pololu_3419_VL53L8CX_Carrier_THT` (PR #44) | main `61f1829` | prior sourced facts; cross-checked below |

## Board identity

| Fact | Tag | Source |
|---|---|---|
| Pololu #3419 = "VL53L8CX Time-of-Flight 8×8-Zone Distance Sensor Carrier with Voltage Regulators, 400cm Max" | DS | P1 |
| Bottom silkscreen shows "Pololu", "© 2024", dev code "irs19b" and a number "J14398" | DS | P1 photo 0J12379 |
| Prior project records (R1, issue #16) say the board in hand is #3419. **No photograph of the physical board is in this repository.** | UNRESOLVED | Gate A item A1 |

## Pololu #3419 carrier

| Fact | Tag | Source |
|---|---|---|
| Two rows: 1×9 (J1) and 1×4 (J2), 0.1″ pitch. J1 pins 1–9: CORE/IOVDD, AVDD, VIN, GND, SDA/MOSI, SCL/MCLK, MISO, NCS, INT. J2 pins 1–4: GND, SPI_I2C_N, LPn, SYNC | DS | P2 schematic connector numbering; P1 photo 0J12380 labels |
| J2 is aligned with J1 pins 4–7 (GND next to GND); the two GND pads are square; mounting holes are on the J2 side | DS | P1 photos 0J12380/0J12379 |
| Board 0.5 × 0.9 in; holes Ø0.086 in, 0.7 in apart; rows 10.16 mm apart (footprint, from Pololu dimension drawing) | DS / R1 | P1 0J12379; R1 footprint |
| Header bodies under the board, pins soldered on the sensor side, sensor facing up (breadboard mounting) | DS | P1 photo 0J12378 |
| Headers are **not included** ("available separately") | DS | P1 |
| VIN 3.2–5.5 V; two regulators make AVDD 3.3 V and CORE/IOVDD 1.8 V; "at VIN below about 3.4 V, AVDD drops below its nominal 3.3 V" | DS | P1 |
| CORE/IOVDD and AVDD are regulator **outputs** (≈50 mA / ≈200 mA available for external parts) | DS | P1 |
| All I/O are level-shifted to VIN by an NXS0108; the shifter pulls all I/O lines high by default (10 kΩ OE pull-up on the schematic) | DS | P1, P2 |
| SPI/I2C is pulled high = SPI mode by default; drive low for I²C ("connect it to the adjacent GND pin, e.g. with a pair of header pins and a shorting block") | DS | P1 |
| LP pulled high = I²C enabled | DS | P1 |
| Default I²C address 0b0101001 (0x29); changed address lasts only until reset/power-off | DS | P1, S1 §4 |
| Typical active current 100 mA, peak 150 mA | DS | P1 |
| Level shifter "more sensitive to external loads": keep wires short, ideally < 8 cm (3 in), few devices; tens of pF can damp oscillation | DS | P1 |
| May ship with a protective liner over the sensor that must be removed | DS | P1 |
| The two regulator part numbers are not named by Pololu; their dropout at our load is **unknown** | UNRESOLVED | P1, P2 (unlabelled 5-pin LDOs) |

## VL53L8CX

| Fact | Tag | Source |
|---|---|---|
| AVDD 3.13–3.47 V (3.3 V nom); CORE_1V8 1.62–1.98 V; IOVDD 1.8 V or 1.2 V | DS | S1 Tables 9–10 (via R1) |
| I²C up to 1 MHz; device address 0x52/0x53 (8-bit) = 0x29 (7-bit) | DS | S1 §4, Table 6 |
| A message may end with a stop **or** a repeated start; a read returns the register at the *current* index; auto-increment | DS | S1 §4 (I²C control interface) |
| `vl53l8cx_is_alive()` = write 0x7FFF←0x00, read regs 0 and 1 expecting 0xF0 / 0x0C, write 0x7FFF←0x02 | CODE | C2 `vl53l8cx_api.c` |
| `vl53l8cx_init()` uploads ~84 KB of firmware every start-up | DS | S2 §4.1 |
| Reset = remove VDDIO, AVDD and CORE_1V8 for 10 ms; toggling SPI_I2C_N resets only the I²C interface | DS | S2 §4.2 |
| Target status table (0–13, 255); status 5 = 100 % valid, 6 or 9 ≈ 50 % confidence, others below 50 % | DS | S2 Table 4 |
| Driver errors: 0 ok, 127 invalid setting, 255 major error (usually an I²C/SPI timeout) | DS | S2 Table 5 |
| Range accuracy (continuous; full-FoV target, AVDD 3.3 V, no cover glass, 2.7σ): 200–4000 mm ±3–7 % at 4×4/30 Hz, ±5–8 % at 8×8/15 Hz; corners up to 4 % worse; add 1–2 % for assembly tilt | DS | S1 Table 18 + notes |
| Max ranging frequency 15 Hz at 8×8, 60 Hz at 4×4 | DS | S2 Table 2 (via R1) |

## UNO Q

| Fact | Tag | Source |
|---|---|---|
| Qwiic QWIIC1: 1 GND, 2 +3V3 OUT (`PWR_3P3V`, "Supply for Qwiic devices"), 3 SDA = PD13 (I2C4_SDA), 4 SCL = PD12 (I2C4_SCL), 3.3 V domain | DS | A1 §9.3 |
| Schematic: QWIIC1 = CI1104M1HR0-NH; pin 2 tied **directly** to PWR_3P3V (no fuse or switch); SDA/SCL through 0 Ω (R2615/R2616); 2.2 kΩ pull-ups R2613/R2614 to PWR_3P3V; 100 nF C2610 | DS | A2 sheet 20 |
| Qwiic is on the top side, at the board edge opposite USB-C, next to the JSPI 2×3 header; pin 1 marked on the pinout drawing | DS | A3 p.1 |
| "All MCU GPIOs are 3.3 V logic and 5 V tolerant, except A0 and A1" | DS | A3 p.1 |
| PWR_3P3V: 3.1 V min / 3.3 V typ / 3.5 V max. A TPS62A02A buck makes it from the 3.8 V pre-regulator. It supplies the STM32U585, the ANX7625 3.3 V rails, the Wi-Fi 3.3 V domain and the 3.3 V header pins | DS | A1 §4 power tables; A2 sheet 22 |
| TPS62A02A: 2.5–5.5 V input, **2 A** step-down | DS | T1 |
| No documented current allowance for external loads on the 3.3 V header/Qwiic pins | DS (absence) | A1, A4 §5.8 ("depends on host board") |
| USB-C requests 5 V / 3 A only | DS | A1 |
| The MCU manages the 3.3 V headers JDIGITAL, JANALOG, JSPI and Qwiic; Linux (QRB2210) owns 1.8 V SoC I/O, CSI, DSI | DS | A1 §5–6 |
| JMISC also carries I2C4 on PF14 (SCL) / PF15 (SDA): the same peripheral as Qwiic | DS | A1 §9.1, A2 |
| Core 1.0.0 variant: `i2cs = <&i2c2>, <&i2c4>, <&i2c3>` → **`Wire` = I2C2 (D20/D21), `Wire1` = I2C4 (Qwiic, default pinctrl PD12/PD13), `Wire2` = I2C3 (A4/A5)**; I2C4 at `I2C_BITRATE_FAST` (400 kHz) | CODE | C1 overlay + `Wire.cpp` DECLARE_WIRE_N |
| I2C4 `pinctrl-1` ("sleep") = PF14/PF15, but the variant `.conf` does not enable `CONFIG_PM_DEVICE`, so the sleep state is not applied | CODE / DERIVED | C1 |
| Zephyr `Wire`: 256-byte TX/RX rings; `endTransmission(stop)` and `requestFrom(..., stop)` **ignore the stop flag** (always STOP); `requestFrom` > 256 bytes returns 0 | CODE | C1 `Wire.cpp` |
| Sketches upload with `remoteocd` over ADB on USB (or `remoteocd_network`), flashing loader + sketch; sketch at 0x08100000, max 786,432 bytes | CODE | C1 `platform.txt`, `boards.txt` |

## Bridge / monitor path

| Fact | Tag | Source |
|---|---|---|
| `Bridge` is an RPC layer over a UART; default 115200 baud | CODE | C3 `bridge.h` `DEFAULT_SERIAL_BAUD` |
| `Monitor.write()` drops data when no monitor client is connected; each write is one RPC (`mon/write` notify, or call in compatibility mode) | CODE | C3 `monitor.h` |
| `Monitor.read()` returns only already-fetched bytes; `available()` triggers the `mon/read` RPC | CODE | C3 `monitor.h` |
| arduino-router: serial 115200 default, Unix socket `/var/run/arduino-router.sock`, **monitor proxy TCP 127.0.0.1:7500**; with several monitor clients each write gets a 500 ms deadline | CODE | C4 |

## Media Carrier coexistence

| Fact | Tag | Source |
|---|---|---|
| The Media Carrier connects through JMEDIA + JMISC (passthrough) and provides 2× CSI and 1× DSI, all in the 1.8 V MPU domain | DS | A4 |
| The ToF path uses only MCU-domain pins (Qwiic). No signal is shared with CSI/DSI/CCI | DERIVED | A1, A4 |
| Shared resources: 5 V input budget, PWR_3P3V capacity, ground, mechanics, Linux CPU for the logger | DERIVED | — |
| A B0394 IMX219 streamed via camss on CAMERA1 on 2026-10-09 (PR #48, unmerged; depends on PR #42 overlay/rebind fixes) | REPO | PR #48 body |
| Simultaneous camera + display + ToF operation on this unit | UNRESOLVED | Gate F |

## Derived: supply headroom at VIN = UNO Q 3.3 V

1. Pololu VIN minimum is 3.2 V (P1). The UNO Q rail is specified at 3.1–3.5 V (A1), so
   at the rail's specification minimum the carrier would be **below** its own minimum.
   The nominal 3.3 V is inside it.
2. AVDD = VIN − (carrier LDO dropout at the sensor's AVDD load). Pololu's note that AVDD
   falls below 3.3 V when VIN < ~3.4 V implies roughly 0.1 V of dropout under its
   (unstated) test conditions. At VIN = 3.30 V that **suggests** AVDD ≈ 3.2 V, about
   0.07 V above the sensor's 3.13 V minimum. Neither the LDO part number nor its dropout
   curve is published, so this margin is an inference, not a specification.
3. Cable drop (assumption: 28 AWG, ≈0.21 Ω/m per conductor, typical for Qwiic cables but
   **not stated** for Adafruit #4397): 0.15 m × 2 conductors × 0.21 Ω/m × 0.15 A ≈ 9 mV.
   This is small against the margin in item 2.
4. Load: 100 mA typical / 150 mA peak (P1) on a 2 A buck (T1) that also feeds the MCU,
   bridge chip and Wi-Fi. Arduino documents no external allowance, so there is no
   documented basis to say the load is acceptable, nor any documented prohibition.
   It is measured at Gate C.
5. **Conclusion:** the documentation permits VIN = 3.3 V (inside Pololu's 3.2–5.5 V and
   the sensor's AVDD range if the dropout is ≈0.1 V), but it does **not guarantee**
   margin. Gate C must measure VIN and AVDD *while ranging 8×8 continuously*.

## Derived: why VIN must not simply move to 5 V

The NXS0108 B-side I/O follow VIN, with internal pull-ups (R1 ICD §5a: RPU 4 kΩ when
high, from the Nexperia datasheet). With VIN = 5 V on the Qwiic bus, the line high level
would be set by the UNO Q's 2.2 kΩ to 3.3 V in parallel with 4 kΩ to 5 V. That gives
≈ 3.3 + 1.7 × 2.2/6.2 ≈ 3.9 V, below the NXS0108 B-side VIH of VCC(B) − 0.4 V = 4.6 V
(R1 ICD §5a). Logic would be unreliable. The STM32 pins are 5 V tolerant (A3), so this
is a function problem, not a damage problem, but it rules out 5 V VIN on the shared
3.3 V bus without a separate 3.3 V↔5 V I²C translator (fallback C in the README).
