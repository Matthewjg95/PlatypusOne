# ICD — UNO Q / Media Carrier ↔ Perception Head (Rev A, preliminary)

Status: **PRELIMINARY, 2026-10-05.** Logical functions are stable; physical
connector family, pin order and host pins are **not frozen** (`TBD_*`).
Facts and sources: [RESEARCH_LEDGER.md](RESEARCH_LEDGER.md). Schematic:
`perception_head_rev_a/host_interface.kicad_sch`.

## 1. Architecture of the link

```
UNO Q (STM32U585 MCU domain, 3.3 V)                 Perception Head Rev A
  Qwiic I2C4 (PD12/PD13) ───── I2C_A ──────────────► BMI270 · AT24CS32 · TCA9534
  JDIGITAL I2C2 (D21/D20) ──── I2C_B ──────────────► Pololu #3419 (VL53L8CX) only
  JDIGITAL GPIO (EXTI) ◄────── TOF_INT_N, IMU_INT1
  JDIGITAL GPIO/timer ──────── TOF_SYNC ──────────► ToF SYNC (optional trigger)
  JANALOG A0 (DAC) / PWM ───── ILLUM_SET ─────────► illumination set-point
  5V_USB_VBUS ──────────────── +5V_HOST ──────────► ToF LDO, LED switch
  PWR_3P3V ─────────────────── +3V3_HOST ─────────► logic only (few mA)
Media Carrier: CSI0/CSI1 ◄── FFC ── IMX219 module (mechanically on the head; electrically NOT on the head)
               DSI0      ── display (unaffected)
```

Rules:
- The head uses **only MCU-domain 3.3 V signals**. No 1.8 V SoC GPIO, no CCI
  I2C, no JMEDIA signals. ⇒ no electrical interaction with CSI/DSI/touch.
- Slow controls (ToF power, illumination enable, EEPROM WP, status LED) are on
  the head's TCA9534 over I2C_A, so the cable carries only power, buses and
  time-relevant lines.
- Every head function fails *off*: ToF rail, illumination switch and LED
  set-point all default to off with the host absent, unpowered or in reset.

## 2. Signal table (logical)

| Head net | Dir (head view) | Level | Function | Rev A host candidate | Freeze |
|---|---|---|---|---|---|
| +5V_HOST | power in | 5 V | ToF LDO input + illumination | JANALOG pin 5 `5V_USB_VBUS` (H1) | TBD_HOST_5V |
| +3V3_HOST | power in | 3.3 V | IMU, EEPROM, expander, op-amp | Qwiic pin 2 or JANALOG pin 4 | TBD_HOST_3V3 |
| GND | — | 0 V | return (3 contacts) | JANALOG 6/7, Qwiic 1 | — |
| I2C_A_SCL / SDA | bidir | 3.3 V OD | bus A: BMI270 0x68, AT24CS32 0x50 + 0x58, TCA9534 0x20 | Qwiic I2C4 PD12/PD13 (2.2 k on UNO Q) | candidate strong |
| I2C_B_SCL / SDA | bidir | 3.3–3.43 V OD | bus B: VL53L8CX 0x29 only | D21/D20 I2C2 PB10/PB11 | TBD_HOST_I2C_B |
| TOF_INT_N | out | 3.43 V (sensor open-drain, pulled up/driven through the Pololu shifter) | ToF data-ready (active low by default config) | D2 / PB3 (EXTI3) | TBD_INT |
| IMU_INT1 | out | 3.3 V (BMI270 push-pull, configurable) | IMU data-ready / motion | D7 / PB2 (EXTI2) | TBD_INT |
| TOF_SYNC | in | 3.3 V | ToF single-acquisition trigger (optional, timing experiments) | D9 / PB8 (TIM4_CH3) | TBD_SYNC |
| ILLUM_SET | in | 0–3.3 V analog | LED current set-point, 3.3 V → 254 mA/ch | A0 / PA4 DAC (alt: PWM ≥20 kHz on D5/D6) | TBD_PWM |

Head-internal (expander) functions: `TOF_PWR_EN` (P0), `TOF_LPN` (P1, input
unless re-addressing), `TOF_I2C_RST_OPT` (P2, spare), `ILLUM_EN_REQ` (P3, → ILLUM_EN via R27),
`EEPROM_WP` (P4), `ILLUM_FAULT_N` (P5, in), `IMU_INT2` (P6, in), `STATUS_LED` (P7).

Candidate host pins avoid: D0/D1 (UART), D10–D13 (SPI2, kept for a future SPI
ToF), D3 (TT pad), and leave a timer CH1/CH2 pair (e.g. TIM3 on D8/PB4 + A2/PA6)
free for the rotary encoder on the host carrier. They are candidates only (H6).

**Board identification:** no strap pin. Identity = AT24CS32 128-bit serial +
record; "head present" = ACK at 0x50/0x20 on bus A.

**Synchronisation:** Rev A target is still capture; host timestamps ToF frames
(INT edge) and IMU (INT1) on the MCU clock. TOF_SYNC is kept because one wire is
cheaper than a respin if C2 says triggered ToF acquisition is needed.
**Debug:** all of the above on test points on the head; no separate debug
connector (SWD stays on the UNO Q).

## 3. Rev A connector J1 (PROVISIONAL)

JST GH, 1.25 mm, 14-pin, SMT horizontal `SM14B-GHS-TB`, mates `GHR-14V-S`.

| Pin | Net | | Pin | Net |
|---|---|---|---|---|
| 1 | +5V_HOST | | 8 | I2C_A_SDA |
| 2 | +5V_HOST | | 9 | I2C_B_SCL |
| 3 | GND | | 10 | I2C_B_SDA |
| 4 | GND | | 11 | TOF_INT_N |
| 5 | +3V3_HOST | | 12 | TOF_SYNC |
| 6 | GND | | 13 | IMU_INT1 |
| 7 | I2C_A_SCL | | 14 | ILLUM_SET |

**Can one connector carry it safely?** Yes. Worst-case 5 V current is bounded
by the head's own limiter (U3 ≤0.71 A) + ToF (≤0.15 A) ≈ 0.86 A over two GH
contacts rated 1 A each; 3V3 draw is a few mA. 14 of 15 available GH positions
are used. The bench harness from J1 splits to Qwiic + 0.1" header pins at the
UNO Q end; the future host carrier mates J1 directly.

J2 (Qwiic, JST SH 4-pin) carries bus A + 3V3 only, for bench bring-up of
IMU/EEPROM/expander without the full harness. Never connect J1 and J2 to two
different hosts.

## 4. Connector family comparison

| Family | Pitch | Lock | Current/contact | Positions | Assembly / service | Cable flexibility in folded sheet metal | Maker access | Verdict |
|---|---|---|---|---|---|---|---|---|
| **JST GH** | 1.25 mm | positive latch | 1 A (#26 AWG) | 2–15 | SMT, hand-solderable; crimp or pre-made leads | round wires, flexible, ~3 mm bundle | Very high (drone/Pixhawk ecosystem) | **Rev A bench + likely host carrier** |
| JST SH / Qwiic | 1.0 mm | friction | 1 A | 4 std | SMT; pre-made Qwiic cables | good | Very high | bus-A bench port only (no lock, too few pins) |
| Molex Pico-Lock | 1.0/1.5 mm | positive | up to ~2.5 A (1.5 mm) | 2–15 | SMT, low profile | good | Medium | Production candidate if GH height is a problem |
| Molex PicoBlade | 1.25 mm | friction | 1 A | 2–15 | SMT/THT | good | High | Reject (no positive lock in a handheld) |
| FFC/FPC 0.5 mm + ZIF | 0.5 mm | actuator | ~0.5 A | 10–40 | SMT ZIF both ends; fragile on repeated service | **best** through tight bends, flat | Medium (custom length FFC) | Production candidate once bend radii known (H7/H8) |
| Custom flex (head tail) | — | ZIF/B2B | — | any | rigid-flex cost | best | Low | Rev B/production only |
| 0.1" Dupont | 2.54 mm | none | 3 A | any | trivial | poor | Very high | Host-side bench harness only |

Do not freeze until Fusion supplies route length, bend radii and service access (H7, H8).

## 5. Electrical notes for the host side

- Bus A pull-ups are on the UNO Q (2.2 kΩ). Head pull-ups R1/R2 are DNP.
- Bus B high level is set by the Pololu shifter (10 kΩ to 3.43 V) plus any host
  pull-ups (H4). Host STM32 pins are 5 V-tolerant FT in OD mode; 3.43 V is also
  inside VDD + 0.3 V.
- With ToF powered off, keep TOF_SYNC low/Hi-Z and expect bus B to read low
  (shifter unpowered) — firmware must treat bus B as absent while TOF_PWR_EN=0.
- ILLUM_SET floating or 0 V ⇒ LEDs off (R17 pull-down). If driven by PWM,
  use ≥20 kHz so R16/R17/C13 (τ ≈ 1.9 ms) leaves <1 % ripple.
- Hot-plug of J1 with the host powered is **not** a supported Rev A operation
  until P3 is measured.

## 6. What the host carrier (board B) must provide

See [HOST_CARRIER_ARCHITECTURE.md](HOST_CARRIER_ARCHITECTURE.md): a mating J1
with the same logical signals, a 5 V rail with ≥1 A allocated to the head, a
3.3 V logic reference tied to the UNO Q PWR_3P3V domain (pull-up coherence),
and routing of the candidate MCU pins once frozen.
