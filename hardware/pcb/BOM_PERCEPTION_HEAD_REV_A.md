# Perception Head Rev A — candidate BOM with rationale

Machine-readable BOM (exported from the schematic, grouped, with DNP flags):
[`perception_head_rev_a/outputs/perception_head_rev_a_bom.csv`](perception_head_rev_a/outputs/perception_head_rev_a_bom.csv).
This page explains **why** each active/selected part is there and what the
fallback is. Lifecycle checked on manufacturer pages 2026-10-05. Stock: one JLCPCB parts-library snapshot (2026-10-05, table below). Redo at 2–3 distributors on order day. Status: SELECTED (use unless
evidence says otherwise) · CANDIDATE (reasonable, still comparing) ·
PROVISIONAL (placeholder until a gate closes).

## Active parts and modules

| Ref | Part (MPN) | Package | Status | Why this part | Verified against | Alternate |
|---|---|---|---|---|---|---|
| M1 | Pololu **#3419** VL53L8CX carrier | 12.7 × 22.9 mm, 0.1" headers | SELECTED (Rev A) | Same hardware as Platypus Lab evidence; solves 3-rail supply + 1.8 V I/O; swappable | Pololu drawing + schematic; custom footprint from drawing | Pololu VL53L7CX carrier (pin-compatible, 90° FoV); bare VL53L8CX = Rev B |
| U1 | TI **TLV75801PDBVR** | SOT-23-5 | SELECTED | Adjustable (sets 3.29 V; 3.43 V fallback), 1 % ref, active discharge for clean ToF reset, EN, 500 mA / 350 mA ISC, ACTIVE | DS pinout = KiCad `TLV75801PDBV` symbol | TLV75533P (fixed 3.3 V, less AVDD headroom) |
| U2 | Bosch **BMI270** | LGA-14 2.5 × 3.0 | SELECTED | Current Bosch IMU, 3.3 V-compatible VDD/VDDIO, I2C, 2 INTs, low current; BNO055 is NRND | Pinout Table 22 (= BMI160 symbol); Bosch land pattern → custom footprint | BMI323 (newer, different pinout/package — would need new footprint) |
| U3 | TI **TPS2553DBVR** | SOT-23-6 | SELECTED | Current-limited, soft-start, reverse-blocking 5 V switch with FAULT: protects host 5 V from LED faults; EN default off | SLVS841F pin table → custom symbol | TPS22918 (no current limit) + polyfuse |
| U4 | TI **TLV9062IDR** | SOIC-8 | SELECTED | RRIO dual, 3.3 V supply, 10 MHz, low offset → accurate low-voltage current sense loop; hand-reworkable package | DS; stock `TLV9062xD` symbol | TLV9002 (lower GBW), MCP6002 |
| U5 | Microchip **AT24CS32-STUM-T** | SOT-23-5 | SELECTED | 4 KB + factory 128-bit unique serial + WP pin: identity and compact calibration in one part | Family DS (AT24CS04/08) pinout = stock symbol; AT24CS32 DS to confirm serial read (C4) | 24AA025E48 (EUI-48, 256 B) |
| U6 | TI **TCA9534PWR** | TSSOP-16 | SELECTED | 8 I/O over bus A, no internal pull-ups (defaults defined by our resistors), INT, 5 V-tolerant | DS pin table = stock symbol | PCA9534 (NXP), PCAL6408A |
| Q1, Q2 | Nexperia **BCP56-16** | SOT-223 | CANDIDATE | 80 V/1 A NPN pass element; BJT linear-mode SOA; cheap, common | Stock symbol (B1 C2 E3 C-tab) | BCX56 (SOT-89, less dissipation); logic MOSFET in DPAK if dissipation grows |
| D1 | **SMF5V0A-E3-08** (Vishay) | SMF (SOD-123F) | CANDIDATE | 5 V unidirectional TVS on cable 5 V input | Footprint stock `D_SMF` | SMBJ5.0A (bigger) |
| D2 | Green LED 0603 | 0603 | GENERIC | Firmware heartbeat/status | — | any |

## Stock snapshot (JLCPCB parts library, 2026-10-05)

| MPN | Stock | LCSC code | Note |
|---|---|---|---|
| TLV75801PDBVR | 164,219 | C2877852 | — |
| TPS2553DBVR | 57,938 | C55266 | — |
| TCA9534PWR | 15,089 | C783615 | — |
| TLV9062IDR | 170,048 | C398355 | — |
| BMI270 | 8,032 | C2836813 | — |
| BCP56-16,115 | 32,184 | C92221 | — |
| SMF5V0A-E3-08 | 12,353 | C1972946 | (SMF6V0A-E3-08: 34,287, if 5.5 V VBUS leakage matters) |
| **AT24CS32-STUM-T** | **52** | C147891 | **thin**: SOIC 28, UDFN 261; alternate 24AA025E48T-I/OT 6,428 (C129895). See register C5 |
| SM14B-GHS-TB(LF)(SN) | 3,288 | C265343 | genuine JST |
| SM04B-SRSS-TB(LF)(SN) | thin (genuine) | — | compatible clones plentiful, e.g. HC-1.0-4PWT 108,747 |
| SM02B-GHS-TB(LF)(SN) | 2 (genuine) | C189893 | compatible clones plentiful, e.g. A1257WR-S-2P 63,705 |
| Pololu #3419 | not checked | — | buy from Pololu directly |

## Connectors / mechanical

| Ref | Part | Status | Why | Alternate |
|---|---|---|---|---|
| J1 | JST **SM14B-GHS-TB(LF)(SN)** (mates GHR-14V-S) | PROVISIONAL | Locking, 1 A/contact, 14 signals in one cable, cheap ready-made leads | Molex Pico-Lock, 0.5 mm FFC — after Fusion routing (H8) |
| J2 | JST **SM04B-SRSS-TB(LF)(SN)** | SELECTED | Qwiic bench port for bus A | — |
| J3, J4 | JST **SM02B-GHS-TB(LF)(SN)** | PROVISIONAL | Locking 2-wire LED light-board links | Pads + soldered leads |
| JP1 | Solder jumper (open) | SELECTED | Hard illumination kill | — |
| H1–H3 | M2 holes (datum/slot/clamp) | PROVISIONAL | Locate-then-clamp head to datum bracket | Dowel pins (C3 result) |
| H4–H7 | M2 holes (camera standoffs) | PROVISIONAL | Head carries the camera module | Pattern from measured module (M2) |
| FID1–2 | Fiducials | SELECTED | Cheap insurance for machine assembly | — |

## Passives with design meaning (generic parts otherwise)

| Ref | Value | Function | Basis |
|---|---|---|---|
| R5 / R6 | 49.9 k / 10 k (**0.5 %**) | ToF rail 3.29 V (52.3 k → 3.43 V conditional fallback, ICD §5a) | 0.55 × (1 + R5/R6) |
| R7 / R13 | 100 k | ToF rail and LED switch OFF at power-up | EN pull-downs |
| R15 | 40.2 k (1 %) | U3 limit ≈0.65 A nom | TPS2553 eq. 1 |
| R16 / R17 / C13 | 120 k / 10 k / 220 n | ILLUM_SET → 0–0.254 V, PWM filter, OFF when floating | divider ratio 1/13 |
| R22 / R23 | 1 Ω 1 % 1206 (≥0.25 W) | LED current sense: 1 mV = 1 mA | — |
| SH1 / SH2 / SH3 | 0.05 Ω 1206 / 0.1 Ω 0805 / 1 Ω 0603 | Head 5 V / ToF / logic current measurement | Kelvin TPs |
| R9, R11 | 0 Ω | I2C mode strap (ToF), IMU address 0x68 | — |
| R24 | 10 k | EEPROM write-protected by default | — |
| R27 | 1 k | expander → ILLUM_EN series, lets JP1 override safely | — |
| R28–R30 | 100 k | defined levels for expander P2, IMU INT2/INT1 before firmware config | — |

DNP options (fitted footprints, unpopulated): R1–R4 bus pull-ups, R8 ToF
force-on, R10 expander-controlled ToF I2C reset, R12 IMU 0x69, C15/C16 loop
compensation, C18/C19 bus-B damping.

## Not on this board (listed so they are not forgotten)

| Item | Status | Note |
|---|---|---|
| IMX219 camera module (B0394 / B0393 / B0390) | BLOCKED on #40 | Mounted to H4–H7; FFC to Media Carrier CSI |
| White LEDs (light boards) | OPEN (L6) | High-CRI (≥90) mid-power 3030/3535 class; choose after L2/L3 |
| Light-board PCB / diffuser / baffle | OPEN | Tiny separate board; geometry from Fusion + glare tests |
| Harness J1 → UNO Q (Qwiic + 0.1" pins) | Rev A bench | GHR-14V-S pre-crimped leads |
