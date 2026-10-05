# Perception Head Rev A — research ledger

Facts used by the Rev A schematic, with the source they came from. Collected
2026-10-05. Tags: **DS** = manufacturer document, **DERIVED** = our inference
from DS facts (reasoning shown), **VENDOR** = distributor/module-maker
document, **REPO** = preserved PlatypusOne/Platypus Lab evidence.
Nothing here is a bench measurement.

## Sources

| ID | Document | Version / date | Where |
|---|---|---|---|
| S1 | Arduino UNO Q (ABX00162) datasheet / user manual | "Modified 17/06/2026" | docs.arduino.cc/resources/datasheets/ABX00162-datasheet.pdf |
| S2 | UNO Q schematics | sheet date 01/10/2025 | docs.arduino.cc/resources/schematics/ABX00162-schematics.pdf |
| S3 | UNO Media Carrier (ASX00083) datasheet / user manual | "Modified 05/10/2026" | docs.arduino.cc/resources/datasheets/ASX00083-datasheet.pdf |
| S4 | ST VL53L8CX datasheet DS14161 | Rev 7, Aug 2023 | ST; retrieved via Pololu mirror pololu.com/file/0J2029 (st.com refused this environment) |
| S5 | ST UM3109 (VL53L8CX ULD guide) | Rev 7, Apr 2024 | Pololu mirror pololu.com/file/0J2030 |
| S6 | Pololu #3419 product page, schematic, dimension drawing | drawing 14 Feb 2024 (dev code irs19b) | pololu.com/product/3419 |
| S7 | Bosch BMI270 datasheet BST-BMI270-DS000 | rev 1.6 (doc -08) | bosch-sensortec.com |
| S8 | TI TPS2552/TPS2553 SLVS841F | Aug 2016 | ti.com/lit/ds/symlink/tps2553.pdf |
| S9 | TI TLV758P datasheet | current | ti.com/lit/ds/symlink/tlv758p.pdf |
| S10 | TI TCA9534 datasheet | Rev D | ti.com/lit/ds/symlink/tca9534.pdf |
| S11 | TI TLV9062 datasheet | current | ti.com/lit/ds/symlink/tlv9062.pdf |
| S12 | Microchip AT24CS04/08 datasheet (same CS family as AT24CS32) | — | AT24CS32 DS20006087 download refused (403); family doc used, see open item |
| S13 | Platypus Lab issue #3 + `docs/TOF_TEST_PLAN.md` | 2026-10-05 | Matthewjg95/platypus-lab |
| S14 | PlatypusOne issues #33/#40/#41, PRs #21/#42/#43 | 2026-10-04/05 | this repo |

Status probes on TI product pages (2026-10-05): TPS2553, TLV755P/TLV758P,
TCA9534, TLV9062 all **ACTIVE**. BMI270 is Bosch's current IMU; BNO055 is
marked not recommended for new designs (already recorded in the gates doc).
Distributor stock was **not** checked from this environment — re-check on order day.

## UNO Q / Media Carrier

| Fact | Tag | Source |
|---|---|---|
| Qwiic connector = I2C4 on **PD13 (SDA) / PD12 (SCL)**, 3.3 V, pin 1 GND, 2 +3V3 (PWR_3P3V), 3 SDA, 4 SCL | DS | S1 §9.3 |
| Qwiic lines have 2.2 kΩ pull-ups to 3.3 V on the UNO Q and DF2B7ASL ESD diodes | DS | S2 sheet 19 (R2613/R2614, D26009) |
| JMISC also carries **MCU_I2C4 on PF14 (SCL) / PF15 (SDA)** — a *different pin pair* of the same STM32 I2C4 peripheral, with separate nets (`MCU_I2C4_*_PF14/PF15` vs `*_PD12/PD13`) | DS | S1 §9.1, S2 net names |
| ⇒ Qwiic I2C4 and JMISC I2C4 cannot be treated as two buses; firmware selects one pin pair for I2C4. Use Qwiic for Rev A (existing `Wire1`-style support). | DERIVED | — |
| JDIGITAL D20/D21 = **I2C2 SDA/SCL (PB11/PB10)**, 3.3 V, FT (5 V tolerant input); 2.2 kΩ pull-ups appear on the I2C2 header nets | DS | S1 §9.6, S2 (R2642/R2643 near `MCU_I2C2_*_CONN`) — pull-up assignment BENCH_VERIFY |
| JANALOG A4/A5 = I2C3 (PC1/PC0) — third MCU I2C option | DS | S1 §9.7 |
| JDIGITAL PWM pins: D3 (PB0, TT 3.6 V-tolerant only), D5 (PA11), D6 (PB1 TIM3_CH4), D9 (PB8 TIM4_CH3), D10 (PB9), D11 (PB15) | DS | S1 §9.6 |
| A0/A1 = PA4/PA5 with **DAC0/DAC1**; not 5 V tolerant | DS | S1 §9.7 |
| MCU domain is 3.3 V; SoC (MPU) GPIO, CCI I2C, CSI/DSI are 1.8 V; level translation mandatory for 1.8 V lines | DS | S1 §2, S3 §5.6 |
| CCI_I2C0/1 and MI2S/DMIC pins are interface-dedicated — never general-purpose | DS | S3 §5, S1 §9 |
| Media Carrier JMISC male header exposes MCU GPIO PE7/PE8, I2C4 (PF14/15), PSSI, trace, OPAMP1 — high-density connector, not a maker header | DS | S3 §5.3 |
| Media Carrier J10 `VCC_PX3_1P8` is "low current, suitable only for I2C-level translation" | DS | S3 §5.2/5.8 |
| Carrier +3V3/+5V_USB current "depends on host board" — **no documented header current allowance** on UNO Q | DS | S3 §5.8, S1 §3 |
| UNO Q power: USB-C requests **5 V / 3 A only**; DC_IN 7–24 V via buck; both diode-OR'd to 5V_SYS; PWR_3P3V 3.1–3.5 V | DS | S1 §3 |
| PWR_3P3V comes from a TPS62A02A buck fed by a second TPS62A02A (5 V→3.8 V); 3.3 V also feeds STM32, ANX7625, Wi-Fi 3.3 V domain | DS | S1 §4.3, S2 sheet 22 |
| ⇒ keep head load on UNO Q 3V3 to a few mA; draw ToF and LED power from 5 V | DERIVED | — |
| JANALOG pin 5 / JSPI pin 2 / JMISC 54,56 are `5V_USB_VBUS` ("pass-through"); when powered from DC_IN, VBUS is sourced only through Q2801 in host/OTG mode | DS/DERIVED | S1 §4.3, §9.4/9.7 — **BENCH_VERIFY** 5 V presence on DC_IN power |
| Only one display output (USB-C DP Alt-Mode or JMEDIA DSI) at a time | DS | S1 §2.2 |
| Head uses only MCU-domain pins ⇒ **no electrical conflict with DSI/CSI/touch**, which are SoC/JMEDIA. Shared resources are 5 V current and mechanics. | DERIVED | — |

## VL53L8CX (bare device)

| Fact | Source |
|---|---|
| Supplies: AVDD 3.3 V (3.13–3.47), CORE_1V8 1.8 V (1.62–1.98), IOVDD 1.2 V (1.08–1.32) **or** 1.8 V (1.62–1.98); abs max AVDD 3.47 V, CORE/IOVDD 1.98 V | S4 Tables 9–10 |
| Apply/remove all supplies together; minimum slew 0.001 V/µs (AVDD), 0.012 V/µs (CORE/IOVDD) | S4 §3.3 |
| All digital signals at IOVDD level ⇒ bare device is **not** 3.3 V-I/O compatible | S4 Table 3 notes, Table 15 |
| Current (typ/max avg): active ranging AVDD 43/50 mA, CORE 50/80 mA, IOVDD 0.003/0.006 mA; peak = avg + 10 mA per rail; HP idle AVDD 1, CORE 3 mA; LP idle µA | S4 Table 12 |
| Continuous mode typ 215 mW; autonomous 8×8 5 Hz 32.3 mW | S4 Tables 13–14 |
| I2C ≤1 MHz (Fm+), 8-bit address 0x52 (7-bit 0x29), address change volatile; SPI ≤3 MHz mode 3 (UM3109 states 20 MHz — **conflict**, use DS 3 MHz) | S4 §4–5, S5 §2.3 |
| Reference I2C circuit: 4.7 µF on AVDD, 100 nF on CORE_1V8, 100 nF on IOVDD, 2.2 kΩ pull-ups on SDA/SCL, 47 kΩ pull-ups on INT, SYNC, LPn; 47 kΩ pull-down on SPI_I2C_N and NCS; RSVD1-3 to GND; thermal pad to GND plane (AN5897) | S4 Fig. 5 |
| INT and SYNC default open-drain; LPn low disables I2C (for address change) | S4 Table 3 |
| **Sensor reset = remove VDDIO, AVDD, CORE for 10 ms**; toggling SPI_I2C_N resets only the I2C interface | S5 §4.2, S4 Table 3 note |
| ULD uploads ~84 KB firmware at every init; resolution 4×4 ≤60 Hz, 8×8 ≤15 Hz | S5 §4.1, Table 2 |
| FoV 45°×45° (65° diag) detection; **collector exclusion zone 57.9°×57.9° (86.6° diag) — cover-glass opening must be ≥ exclusion zone** | S4 §2.2 Table 2 |
| Cover-glass crosstalk immune >60 cm by histogram; below 60 cm crosstalk calibration needed (target ≥600 mm, full FoV) | S4 p.2, S5 §3.2 |
| Range accuracy (continuous): 4×4 ±3–7 %, 8×8 ±5–8 % beyond 200 mm; add 1–2 % for assembly tilt/housing | S4 Table 18 + notes |
| Zone image is flipped H and V by the Rx lens (zone 0 sees top-right of scene) | S4 §7.1.3 |
| Offset drift ~0.1 mm/°C with autocalibration at each ranging start | S4 §7.4 |
| Package 6.4 × 3.0 × 1.75 mm optical LGA16 | S4 Table 1 |

## Pololu #3419 carrier

| Fact | Source |
|---|---|
| VIN 3.2–5.5 V; two LDOs make AVDD 3.3 V and CORE/IOVDD 1.8 V; NXS0108 shifts all I/O to **VIN** level and pulls them up by default (10 kΩ OE pull-up shown) | S6 page + schematic |
| VIN < ~3.4 V ⇒ AVDD falls below 3.3 V (VL53L8CX accepts ≥3.13 V) | S6 |
| Typical active current 100 mA, peak 150 mA | S6 |
| SPI/I2C pin pulled high by default = **SPI mode**; tie low for I2C. LP pulled high = I2C enabled. | S6 |
| Shifter "more sensitive to external loads": keep wires <8 cm, few devices on the bus; tens of pF can damp oscillation | S6 |
| Board 12.7 × 22.9 mm, 1.02 mm thick, sensor top 1.8 mm above board; 1×9 + 1×4 0.1" rows; 2× Ø2.18 mm holes for M2; ±0.3 mm edge, ±0.1 mm drill | S6 drawing |
| Pin-compatible family: VL53L5CX and VL53L7CX (90° FoV) carriers share power/I2C pin arrangement | S6 |

### VL53L8CX decision: host the Pololu #3419 for Rev A

**Decision: option B — host the proven module.** Rationale:

1. **Evidence transfer.** Platypus Lab's canonical experiment (S13) uses this
   exact carrier. Hosting it means Lab ranging/crosstalk/failure evidence
   applies to the head without an electrical-equivalence argument.
2. **Removes three Rev A risks at once:** reflow of an optical LGA module
   (liner, contamination, tilt), a three-rail power-sequenced supply, and
   1.8 V↔3.3 V translation — none of which Rev A exists to learn.
3. **Serviceable and swappable:** a damaged sensor is a module swap; the
   VL53L7CX carrier (90° FoV) is a pin-compatible experiment if Lab evidence
   says 45° is too narrow for the camera FOV (62–75° H).
4. **Costs accepted:** larger/taller footprint, header-mounted pose (mitigated
   by soldered headers + 2× M2 screws and per-unit extrinsic calibration),
   load-sensitive shifter (mitigated by a dedicated bus + DNP damping caps +
   bench scope check), carrier LDO dissipation.

**Rev B path (bare IC), recorded so it is not re-researched:** ST Fig. 5
circuit; AVDD from a 3.3 V LDO with 4.7 µF; CORE_1V8 and IOVDD from one
1.8 V LDO with 100 nF each; 2.2 kΩ I2C pull-ups to 1.8 V; 47 kΩ INT/LPn/SYNC
pull-ups and SPI_I2C_N/NCS pull-downs; host-side 1.8↔3.3 V I2C translation
(e.g. TCA9800/PCA9306-class) or move the bus to 1.8 V; thermal pad + ground
plane per AN5897; cover window per S4 §2.2 exclusion zone; all three rails
switched together for reset. Gate: Rev A shows ToF earns its place and the
carrier's size/pose is the limiting factor.

### Why the ToF gets a dedicated bus and a switchable 3.43 V rail (DERIVED)

- Reset requires a supply cycle (S5 §4.2) → the head must be able to remove
  ToF power without touching IMU/EEPROM.
- A powered-down carrier's shifter pulls its I/O toward a dead rail; on a
  shared bus that would drag the IMU/EEPROM bus. Separate bus B contains it.
- Pololu's own guidance (few devices, short leads) also favours isolation.
- TLV758P (S9): 0.55 V reference ±1 %, 500 mA, ISC 350 mA, **active output
  discharge** (clean 0 V during the 10 ms reset), EN active-high, SOT-23-5
  (IN 1, GND 2, EN 3, FB 4, OUT 5), RθJA 176.9 °C/W (DBV).
- 3.43 V (R5 52.3 k / R6 10 k) keeps carrier AVDD headroom while I/O stays
  below STM32 VDD + 0.3 V. Host bus-B pull-ups (3.3 V) and the shifter's
  pull-ups (3.43 V) differ by ~0.13 V → µA-level cross-current, acceptable.
  **BENCH_VERIFY** AVDD ≥3.13 V during 8×8 ranging and rail "off" voltage
  with bus B idling high (back-feed).

## BMI270

| Fact | Source |
|---|---|
| VDD 1.71–3.6 V, VDDIO 1.2–3.6 V, independent; reset when either falls below minimum; no slew constraint | S7 Table 1, §4.3 |
| Current: 685 µA A+G normal (ODR max), 970 µA performance; VDD = 1.8 V spec point | S7 Table 1 |
| Power-on to interface operational 2 ms; registers accessible 450 µs after POR; **8 kB config upload required after every POR/soft reset** | S7 Table 1, §4.4 |
| I2C: CSB hard-wired to VDDIO; SDO → GND = 0x68, → VDDIO = 0x69; bus load ≤400 pF | S7 §6.3, Table 17–18 |
| Pinout (Table 22): 1 SDO, 2 ASDx, 3 ASCx, 4 INT1, 5 VDDIO, 6 GNDIO, 7 GND, 8 VDD, 9 INT2, 10 OCSB, 11 OSDO, 12 CSB, 13 SCx, 14 SDx — identical to BMI160 | S7 §7.1 |
| Unused aux/OIS pins may be DNC (I2C reference diagram 7.2.3 leaves them unconnected) | S7 Table 22, Fig. 7.2.3 |
| 100 nF on VDD and VDDIO in the connection diagram | S7 §7.2.3 |
| Land pattern: side lands 0.475×0.25 mm, top/bottom 0.25×0.475 mm, inner edges 0.925/0.675 mm from centre | S7 §8.3 → custom footprint (KiCad stock lands are 0.2 mm longer outward) |
| Thermomechanical stress causes offset drift; in-use offset compensation available | S7 §4.x "IOC" |

Placement consequences (DERIVED): rigid board area near the optical datum;
away from mounting screws/flex, LDO and LED-driver heat; axes aligned to the
camera frame and recorded in calibration.

## Illumination

| Fact / reasoning | Tag |
|---|---|
| IMX219 modules use a rolling shutter (row-sequential exposure); PWM dimming at line-time-scale periods produces banding. Analog (DC) current control avoids it. | DERIVED (sensor architecture); BENCH_VERIFY with the chosen module |
| White LED Vf ≈ 2.8–3.3 V at a few hundred mA leaves ~1.5 V headroom on 5 V → linear CC is feasible with a 0.25 V sense drop | DERIVED / typical LED data — confirm with selected LED datasheet |
| TPS2553 (S8): 2.5–6.5 V, 85 mΩ, EN active-high, FAULT open-drain, IOS(nom) = 23950/R^0.977 mA (R in kΩ, 15–232 k), ±6 % at 1.7 A, reverse-voltage protection, SOT-23-6 pins IN 1, GND 2, EN 3, FAULT 4, ILIM 5, OUT 6 | DS |
| R15 = 40.2 kΩ → IOS min/nom/max ≈ 592/647/709 mA (eq. 1) | DERIVED |
| TLV9062 (S11): 1.8–5.5 V RRIO, 10 MHz, Vos ≤1.6 mV, Iq 538 µA/ch, ~50 mA output | DS |
| BCP56-16 (Nexperia): 80 V/1 A NPN, SOT-223; BJTs tolerate linear operation better than small trench MOSFETs | DS family / DERIVED — confirm SOA & Ptot vs copper area at layout |
| Light boards off the head: lets LED angle/position/diffuser/baffle iterate against glare tests without re-spinning the calibrated head | DERIVED |

LED current is **not frozen**: design full-scale is 254 mA/channel because of
host 5 V uncertainty and BJT dissipation (~0.45 W/ch continuous), not because
optics require it.

## Identity / calibration memory

| Fact | Tag |
|---|---|
| AT24CS-family: 128-bit factory-programmed, permanently locked unique serial in a separate address space; WP high inhibits all writes, reads allowed; SOT23-5 has no address pins (A bits must be 0); pinout SCL 1, GND 2, SDA 3, VCC 4, WP 5; 1.7–5.5 V | DS (S12 family document) |
| AT24CS32 specifics (32 Kbit = 4 KB, serial-number read address/sequence) | **RESEARCHABLE NOW — open**: confirm from DS20006087 before firmware (download refused here) |
| TCA9534 (S10): no internal I/O pull-ups (that is TCA9554); POR = all inputs, output register defaults 0xFF; INT open-drain; 400 kHz; 5 V-tolerant I/O | DS |

Recommendation (smallest robust): one 4 KB EEPROM per head holding identity +
a compact versioned record (schema id, board rev/variant, serial, camera id,
ToF carrier id, camera↔ToF extrinsic, intrinsics reference hash/version,
calibration revision/date, manufacturing/test state, CRC). Full calibration
data stays on the host keyed by the serial. Write-protected unless firmware
explicitly lowers WP.

## Platypus Lab ToF evidence status (S13)

As of 2026-10-05: recorder/replay software merged (Lab PR #4); test plan
hardened (never 5 V VIN on a 3.3 V bus). **E0/E1/E2 physical gates not run;
no ranging data exists.** The head therefore uses only datasheet/vendor ToF
facts and leaves every operating-mode decision to Lab evidence.
