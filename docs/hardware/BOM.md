# Platypus One — Bill of Materials (rev A, planning)

Status date: **2026-08-04**. Nothing has been acquired yet; every line is
`PLANNED`. Prices are indicative USD from typical distributors (Arduino Store,
Adafruit, DigiKey, Amazon) — refresh at order time. This BOM doubles as the
"beginning of BOM" required for the Hackster hardware application.

Legend: ACQ column = PLANNED → ORDERED → RECEIVED → TESTED.

## Core compute

| # | Item | Qty | Est. | Source | Notes | ACQ |
|---|---|---|---|---|---|---|
| 1 | **Arduino UNO Q (4G RAM)** | 1 | $59 (or free) | Contest hardware / store.arduino.cc | QRB2210 Linux MPU + STM32U585. Apply for contest hardware first; buy only if not selected (announced Sep 18) | PLANNED |
| 2 | microSD card, 32 GB A1, industrial preferred | 1 | $12 | Amazon/DigiKey | OS image + PlatypusOS data partition | PLANNED |
| 3 | USB-C PD power supply 5V/3A + short USB-C cable | 1 | $15 | Amazon | Bench power during bring-up | PLANNED |

## Display & input

| # | Item | Qty | Est. | Source | Notes | ACQ |
|---|---|---|---|---|---|---|
| 4 | 3.2" ILI9341 320×240 SPI TFT **with resistive touch (XPT2046)** | 1 | $16 | Adafruit 1743 / Waveshare | Matches renderer's 320×240 RGB565 target exactly | PLANNED |
| 5 | Tactile buttons, 6×6 mm | 4 | $2 | DigiKey | Power/home/function keys (sim maps to number keys) | PLANNED |

## Sensing (ShadowScan + measurement apps)

| # | Item | Qty | Est. | Source | Notes | ACQ |
|---|---|---|---|---|---|---|
| 6 | Camera: USB UVC module, OV5640-class autofocus | 1 | $25 | Amazon/AliExpress-alt | UVC keeps the Linux driver path simple (V4L2); MIPI is a stretch goal | PLANNED |
| 7 | VL53L1X time-of-flight distance sensor breakout | 1 | $12 | Pololu 3415 / Adafruit | Distance/level measurement app; I²C via MCU | PLANNED |
| 8 | BNO055 (or ICM-20948) 9-DoF IMU breakout | 1 | $25 | Adafruit 2472 | Angle/level measurement; first ISensor driver | PLANNED |
| 9 | TCS34725 color sensor breakout | 1 | $8 | Adafruit 1334 | Color-measure feature (multi-measure lineage) | PLANNED |
| 10 | White LED, high-CRI + driver transistor | 2 | $2 | DigiKey | Controlled shadow casting for ShadowScan + camera illumination | PLANNED |

## Power (handheld operation)

| # | Item | Qty | Est. | Source | Notes | ACQ |
|---|---|---|---|---|---|---|
| 11 | LiPo battery 3.7 V 2500 mAh with protection | 1 | $15 | Adafruit 328 | Confirm carry-on legality (<100 Wh: fine) | PLANNED |
| 12 | USB-C LiPo charge/boost board, 5 V 3 A out (e.g. PowerBoost-class) | 1 | $20 | Adafruit/DigiKey | UNO Q wants stable 5 V under Linux load | PLANNED |
| 13 | Power switch, SPDT slide | 1 | $1 | DigiKey | | PLANNED |

## Audio & feedback

| # | Item | Qty | Est. | Source | Notes | ACQ |
|---|---|---|---|---|---|---|
| 14 | Piezo buzzer or 1 W speaker + PAM8302 amp | 1 | $6 | Adafruit | UI feedback per IAudioOutput | PLANNED |
| 15 | Vibration motor disc (optional) | 1 | $2 | Adafruit 1201 | Haptic feedback, stretch | PLANNED |

## Enclosure & interconnect (PCBWay scope — $300 contest credit)

| # | Item | Qty | Est. | Source | Notes | ACQ |
|---|---|---|---|---|---|---|
| 16 | Custom carrier/breakout PCB (sensor I²C hub, buttons, power routing) | 1 | ~$60 assembled | **PCBWay** | Designed in **Fusion Electronics** — targets "Best Fusion Electronics Design" prize | PLANNED |
| 17 | Enclosure, 3D-printed resin or CNC | 1 set | ~$120 | **PCBWay** | Modeled in Fusion (.f3d is a required deliverable) | PLANNED |
| 18 | JST-SH/Qwiic cables, headers, M2/M2.5 standoffs + screws kit | — | $15 | Adafruit/Amazon | | PLANNED |
| 19 | Hookup wire, heat-shrink assortment | — | $10 | Amazon | | PLANNED |

## Tools (if not already owned)

| # | Item | Est. | Notes | ACQ |
|---|---|---|---|---|
| T1 | Soldering iron (temp-controlled) + solder + flux | $40 | | PLANNED |
| T2 | Multimeter | $25 | Power bring-up requires it | PLANNED |
| T3 | USB-UART adapter, 3.3 V (FTDI/CP2102) | $10 | Serial console / MCU debug | PLANNED |
| T4 | Calipers, digital | $20 | Enclosure fit checks | PLANNED |

## Software (BOM per contest rubric — tools count)

| Item | Cost | Notes |
|---|---|---|
| Autodesk Fusion (+ Electronics) | Free personal / contest license | Enclosure + carrier PCB + schematics |
| PlatypusOS (this repo) | — | C++20, CMake; own work |
| Arduino App Lab / UNO Q Linux image | Free | Board OS |
| KiCad not used — Fusion Electronics only | — | Keeps rubric points concentrated |

## Totals (excluding tools, excluding contest-provided items)

- If awarded contest hardware + $300 PCBWay credit: **≈ $190 out of pocket**
- Fully self-funded worst case: **≈ $430 + tools**

## Ordering strategy

1. **Now (before Aug 23):** order nothing except optionally the display (#4)
   and IMU (#8) — cheap, long-lead-independent, and enough to de-risk the two
   hardest drivers. Everything else waits on the Sep 18 hardware announcement.
2. **Sep 18, if selected:** UNO Q + PCBWay credit arrive via contest; order
   items 2–15, 18–19 immediately (1-week lead).
3. **Sep 18, if not selected:** buy UNO Q retail same day; PCBWay enclosure
   self-funded — trim #17 to FDM-quality budget (~$60).
4. **Oct:** PCBWay carrier PCB order (after breadboard validation) — allow two
   spins; second spin no later than **Nov 10** to hold the Dec 20 deadline.
