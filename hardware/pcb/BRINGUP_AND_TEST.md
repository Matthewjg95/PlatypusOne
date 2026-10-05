# Perception Head Rev A — bring-up and test strategy

Defined **before layout** so the board is built to be debugged. Reference
designators and test points are from the Rev A schematic.

## 1. Safe power-up configuration (by hardware, no firmware involved)

| Function | Default with host absent / in reset / expander at POR | Mechanism |
|---|---|---|
| ToF rail (+3V3_TOF) | **OFF** | R7 pulls U1 EN low; TCA9534 POR = inputs |
| Illumination 5 V | **OFF** | R13 pulls U3 EN low |
| LED current set-point | **0 mA** | R17 pulls ILLUM_VSET to 0 V when ILLUM_SET floats |
| Illumination hard kill | available | close JP1 → ILLUM_EN tied to GND (R27 isolates the expander) |
| EEPROM | **write-protected** | R24 pulls WP high |
| ToF interface mode | I2C | R9 straps SPI/I2C low |
| IMU address | 0x68 | R11 SDO → GND |
| IMU INT1/INT2, expander P2 | defined low | R30/R29/R28 100 kΩ pull-downs |

**Firmware init order for U6 (mandatory):** write Output register (0x01) = 0x00
→ then Configuration register (0x03) with P0, P3, P4, P7 as outputs; leave P1
(TOF_LPN), P2, P5, P6 as inputs. Writing config first would drive outputs to the
POR value 0xFF (ToF on, LEDs enabled, EEPROM writable).

## 2. Failure isolation (the RGB path must survive everything)

The camera's CSI path never touches this board electrically. The head draws
only from host 5 V and 3V3 and drives only MCU pins. Required demonstrations:

| Fault injected | Expected | Check |
|---|---|---|
| J1 unplugged while Linux captures | CSI preview/capture unaffected | capture before/after, no kernel errors |
| ToF rail off / carrier removed | bus A (IMU/EEPROM/expander) still ACKs; bus B absent | i2c scans |
| ToF hung | power-cycle via P0 (≥10 ms off) recovers it without touching bus A | scope TP6/TP7 |
| LED short on J3 | U3 limits, FAULT (TP26) low, host 5 V stays in spec | scope TP1/TP3 |
| IMU dead | ToF + illumination unaffected | — |
| EEPROM blank/corrupt | software reports "uncalibrated head", capture still allowed | firmware test |

## 3. Bring-up sequence (per board)

Instruments: current-limited bench supply, DMM, scope (≥2 ch), USB-C meter,
UNO Q + harness, Pololu #3419, a dummy LED load (one white LED on a GH lead),
thermocouple or IR thermometer.

| Step | Action | Pass criteria | Stop if |
|---|---|---|---|
| B0 | Unpowered: inspect, photograph both sides; resistance +5V_HOST→GND, +3V3_HOST→GND, TOF_LDO_OUT→GND, +5V_ILLUM→GND | No short (capacitor charge ramps are OK) | <10 Ω sustained |
| B1 | **3V3 only** via J2 (Qwiic) from bench supply 3.3 V, 50 mA limit | TP5 ≈3.3 V, SH3 drop → logic current ≤5 mA; TP6, TP3 ≈0 V | current-limit hit |
| B2 | Replace the bench supply with the UNO Q Qwiic cable on J2; scan bus A | ACK 0x20, 0x50, 0x58, 0x68 | missing device → rework that device only |
| B3 | Expander safe-init (order above); toggle P7 | D2 blinks; TP20 and TP25 stay low until commanded | outputs glitch high |
| B4 | EEPROM: read serial, attempt write with WP high (must fail), WP low (P4) write/readback record | serial non-zero/stable; write blocked/allowed as expected | — |
| B5 | IMU: chip ID, 8 kB config upload, data-ready on IMU_INT1 (TP21) | INT pulses at set ODR; gravity ≈1 g | — |
| B6 | Add **+5V** via J1, 200 mA limit, ToF still off | TP2 ≈5 V; +5V_HEAD current <1 mA; TP3 ≈0 | — |
| B7 | Enable ToF rail (P0) **without** carrier | TP6 = 3.29 V ±2 % (3.23–3.36 V); rise monotonic | out of range → check R5/R6 |
| B8 | Fit Pololu carrier (unpowered), enable rail | TP8 AVDD ≥3.13 V, TP9 1.8 V; ACK 0x29 on bus B | AVDD low → T2 |
| B9 | ULD init + ranging 4×4 @10 Hz then 8×8 @15 Hz, 30 min | SH2 voltage → ToF current (record); AVDD under load; INT on TP17; frames valid | resets, AVDD <3.13 V |
| B10 | Scope I2C_B, TOF_INT_N and TOF_SYNC (open-drain) at the real harness length, 400 kHz and 1 MHz, with host 3V3 at its actual value; repeat with C18/C19 fitted | clean edges; I2C_B low ≤0.15 V at the carrier; high ≥ VCC(B) − 0.4 V; levels match ICD §5a | oscillation → T4 mitigation |
| B11 | ToF reset: P0 low 10 ms → TP6 <0.3 V → re-enable → re-init | recovers every time (20 cycles); bus A unaffected | back-feed holds rail up → T5 |
| B12 | Illumination with dummy LED on J3, JP1 open: ILLUM_EN high, ILLUM_SET 0 → 3.3 V in steps | TP27 (mV) = mA, linear to ~254 mA; no ringing on TP27 (else fit C15) | runaway / oscillation |
| B13 | Thermal at full scale, 5 min continuous | Q1/Q2 rise recorded; LED temp recorded | >80 °C case → duty limit |
| B14 | Limit test: short J3 pins via 1 Ω | U3 limits ≈0.6–0.7 A, FAULT low (TP26), host 5 V stays ≥4.75 V | host resets |
| B15 | Integration: UNO Q + Media Carrier + display + CSI camera + head all running | §2 table; power matrix row recorded | any CSI/DSI regression |

Record every step in an evidence file (photos, scope captures, numbers, board
serial from B4) — same discipline as `docs/contest/evidence/`.

## 4. Current-measurement strategy

| Quantity | How | Resolution with a 4½-digit DMM |
|---|---|---|
| Head 5 V current | V across SH1 (0.05 Ω) at TP1/TP2 | 0.1 mV → 2 mA |
| ToF current | V across SH2 (0.1 Ω) at TP6/TP7 | 0.1 mV → 1 mA |
| Logic current | V across SH3 (1 Ω) at TP4/TP5 | 0.1 mV → 0.1 mA |
| LED current ch1/ch2 | V at TP27/TP28 to GND (1 Ω) | 1 mV = 1 mA |
| System current | USB-C inline meter on UNO Q input | per meter spec |

## 5. Test points (31)

Rails: TP1 +5V_HOST, TP2 +5V_HEAD, TP3 +5V_ILLUM, TP4 +3V3_HOST, TP5
+3V3_LOGIC, TP6 TOF_LDO_OUT, TP7 +3V3_TOF, TP8 TOF_AVDD, TP9 TOF_CORE_1V8.
Ground loops: TP10–TP12. Buses: TP13/14 I2C_A, TP15/16 I2C_B.
Control/interrupt: TP17 TOF_INT_N, TP18 TOF_SYNC, TP19 TOF_LPN, TP20 TOF_PWR_EN,
TP21 IMU_INT1, TP22 IMU_INT2, TP23 ILLUM_SET, TP24 ILLUM_VSET, TP25 ILLUM_EN,
TP26 ILLUM_FAULT_N, TP27/28 LED sense, TP29 EXP_INT_N, TP30 TOF_I2C_RST_OPT,
TP31 EEPROM_WP.

## 6. Pre-layout software tasks that make bring-up fast

- STM32 sketch: bus A/B scans, U6 safe-init, EEPROM serial/record R/W with CRC.
- ToF power-cycle state machine (off ≥10 ms, rail check, ULD init, timeout → retry → report).
- BMI270 config upload + INT1 data-ready.
- ILLUM_SET via DAC with a software ceiling (never above the configured mA); set ILLUM_SET = 0 before toggling ILLUM_EN, then ramp.
- Host-side "head present / calibrated / degraded" status for the UI.
