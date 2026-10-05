# Perception Head Rev A — power tree (preliminary)

Status 2026-10-05. Classes: **KNOWN** (manufacturer document) ·
**MEASURED** (preserved Platypus One bench evidence) · **ESTIMATED** (stated
engineering estimate, reason given) · **UNKNOWN** (needs a measurement).
Do **not** size the battery from this file. Sources: [ledger](RESEARCH_LEDGER.md).

**MEASURED values preserved in the repository: none.** (Searched docs and
evidence folders on 2026-10-05; the power matrix in
[POWER_BATTERY_REFERENCE.md](../../docs/hardware/POWER_BATTERY_REFERENCE.md)
is still all TBD.)

## 1. Rail tree

```
UNO Q 5V_USB_VBUS (JANALOG 5) ──J1.1/2──┬─ D1 SMF5V0A TVS
                                        └─ SH1 0.05 Ω ── +5V_HEAD ──┬─ U1 TLV758P (EN = TOF_PWR_EN, default OFF)
                                                                    │     └─ 3.43 V TOF_LDO_OUT ── SH2 0.1 Ω ── +3V3_TOF ── Pololu #3419 VIN
                                                                    │                                   (carrier LDOs → AVDD 3.3 V, CORE/IOVDD 1.8 V)
                                                                    └─ U3 TPS2553 (EN = ILLUM_EN, default OFF, limit ≈0.65 A)
                                                                          └─ +5V_ILLUM ── J3/J4 light boards ── Q1/Q2 + 1 Ω sense (linear CC sinks)
UNO Q PWR_3P3V (Qwiic 2 / JANALOG 4) ──J1.5── SH3 1 Ω ── +3V3_LOGIC ── BMI270 · AT24CS32 · TCA9534 · TLV9062 · pull-ups · status LED
```

Design intent: almost nothing on the UNO Q's shared 3.3 V buck (which also
feeds the STM32, ANX7625 and Wi-Fi); ToF and LEDs ride on 5 V where the USB-C
contract (5 V/3 A) is the documented limit.

## 2. Head loads

| Load | Rail | Typical | Max / peak | Class | Basis |
|---|---|---|---|---|---|
| VL53L8CX active ranging (via Pololu VIN) | +3V3_TOF ← +5V_HEAD | 100 mA | 150 mA peak | KNOWN | Pololu #3419; consistent with ST DS Table 12 (AVDD 43/50 + CORE 50/80 mA, +10 mA peaks) |
| VL53L8CX HP idle | +3V3_TOF | ~4 mA | ~19 mA | KNOWN | DS Table 12 (AVDD 1/1.6, CORE 3/17 mA) + shifter |
| ToF rail off | — | 0 | back-feed only | ESTIMATED | active discharge LDO; T5 to measure |
| U1 LDO dissipation | — | 0.16 W | 0.24 W (ΔTj ≈ 42 °C @ RθJA 176.9 °C/W) | KNOWN/DERIVED | (5 − 3.43 V) × I |
| BMI270 | +3V3_LOGIC | 0.69 mA | 0.97 mA (performance mode) | KNOWN @1.8 V; ESTIMATED same @3.3 V | Bosch DS Table 1 |
| AT24CS32 | +3V3_LOGIC | µA standby | ≤3 mA during write | ESTIMATED | typical I2C EEPROM; confirm DS20006087 (C4) |
| TCA9534 | +3V3_LOGIC | µA | ≤0.25 mA | ESTIMATED | confirm DS ICC table |
| TLV9062 | +3V3_LOGIC | 1.08 mA | ~1.2 mA | KNOWN | 538 µA/ch |
| Q1/Q2 base drive | +3V3_LOGIC (via op-amp) | 0 | 5 mA at full scale | DERIVED | 2 × 254 mA / hFE(min 100) |
| Status LED D2 | +3V3_LOGIC | 1.3 mA when on | 1.3 mA | ESTIMATED | (3.3 − ~2.0 V)/1 kΩ |
| Pull-ups held low (R14, R24, R26) | +3V3_LOGIC | 0 | 1 mA | DERIVED | 3 × 0.33 mA |
| TPS2553 quiescent | +5V_HEAD | 0.12 mA (on) | 0.14 mA | KNOWN | SLVS841F supply-current table |
| TLV758P ground current | +5V_HEAD | 25 µA | 35 µA | KNOWN | DS IGND |
| Illumination, 2 ch at design full scale | +5V_ILLUM | 0 (off) | 2 × 254 mA = 0.51 A | DESIGN LIMIT (not a requirement) | R16/R17 + 1 Ω |
| — of which LEDs | | | ≈1.5 W | ESTIMATED | Vf ≈ 3.0 V at 254 mA (LED not selected) |
| — of which Q1+Q2 | | | ≈0.9 W | DERIVED | (5 − 3.0 − 0.25 V) × 0.254 A × 2 |
| — of which sense resistors | | | 0.13 W | DERIVED | 0.254² × 1 Ω × 2 |
| Illumination hard cap | +5V_ILLUM | | 0.592–0.709 A | KNOWN/DERIVED | TPS2553 eq. 1 with R15 = 40.2 kΩ |
| Measurement shunts SH1/SH2/SH3 | | | 37 mW / 2 mW / µW | DERIVED | I²R |

### Head totals (DERIVED from the rows above)

| Operating state | +5V_HEAD | +3V3_LOGIC | Head power |
|---|---|---|---|
| Idle, ToF off, LEDs off | ~0.2 mA | ~2–4 mA | ~0.01 W |
| ToF ranging, LEDs off | 100 mA typ / 150 mA peak | ~3–4 mA | 0.51 W typ / 0.76 W peak |
| ToF ranging + LEDs at design full scale | ~0.61 A typ / 0.66 A peak | ~10 mA | ≈3.1 W typ |
| Absolute bound (limiter + ToF peak) | 0.86 A | ~10 mA | ≈4.3 W |

**Margin rule (ESTIMATED, to be replaced by measurement):** allocate 1.0 A at
5 V to the head on the host carrier; Rev A bench may run LEDs at reduced
set-point until H2 shows UNO Q + display + camera leave that headroom.

## 3. System-level placeholders (all UNKNOWN)

| Load | Rail | Status | How it gets measured |
|---|---|---|---|
| UNO Q (Linux idle / camera pipeline / Wi-Fi burst) | 5 V USB-C | UNKNOWN | inline USB-C meter, states below |
| UNO Media Carrier (incl. on-board level shifting) | via UNO Q | UNKNOWN | difference with/without carrier |
| Compact display (3.5–4.3 in, not chosen) + backlight | via carrier / its own 5 V | UNKNOWN | 5 in dev panel as proxy now, re-measure on chosen panel |
| IMX219 camera module (each of B0394/B0393/B0390; B0393 VCM) | Media Carrier camera rail | UNKNOWN | difference preview vs no camera; record per SKU |
| Perception head | 5 V + 3V3 | §2 (datasheet-based) | head shunts SH1/SH3 |
| Battery → 5 V conversion loss | — | UNKNOWN (topology not chosen); ESTIMATED 85–93 % efficiency for a buck from 2S/3S | measure once host carrier power stage exists |

## 4. What Matthew needs to measure (closes the system model)

Use the USBC-VAMETER3 (or equivalent) inline on the UNO Q USB-C input,
5 V/3 A PD source, record 30 s average + max for each state; log ambient temp.

1. **UNO Q alone:** boot peak, Linux idle, CPU-heavy (Scout analyzer run).
2. **+ Media Carrier:** idle.
3. **+ 5 in DSI display:** idle at fixed backlight, kiosk running.
4. **+ CSI camera (B0394 first):** live preview, still capture burst; repeat for B0393 (incl. autofocus sweep) and B0390.
5. **+ Pololu VL53L8CX on Qwiic/3.3 V bench wiring:** 4×4@10 Hz and 8×8@15 Hz continuous (also supplies T2 AVDD and P2).
6. **+ bench LED at fixed current** (e.g. 100/200/250 mA from a CC bench supply into one high-CRI LED): captures for L1/L2 — measure the LED current separately, not through the UNO Q.
7. **5 V presence on DC_IN power** (H1) and DC_IN input current for the same states once a 7–24 V supply is used.
8. **Brown-out check:** run state 4 + ToF + 0.5 A dummy load on 5 V header; note any reset/USB disconnect.

Results go in `docs/hardware/POWER_BATTERY_REFERENCE.md` (matrix) with raw
meter screenshots/logs under the evidence folder; then this file's §3 changes
from UNKNOWN to MEASURED with links.
