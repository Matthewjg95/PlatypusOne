# Host / Power / Controls Carrier (board B) — architecture only

Status: **architecture + interfaces, no layout.** Physical layout waits for
the compact-display decision (#41) and measured system power (P1/H2).
The UNO Media Carrier remains the multimedia reference platform.

## Responsibilities

| Function | Direction | Notes / open items |
|---|---|---|
| Battery + protection + charging | required | Battery **not sized** (POWER_BATTERY_REFERENCE.md gate). Pack class (2S/3S smart pack vs 1S) drives topology. |
| System 5 V rail | required | Feeds UNO Q (5 V pin or DC_IN 7–24 V path — choose after H1), display, head (allocate ≥1 A for head). |
| Power path | required | USB-C charge-while-operating; soft power switch; brown-out safe shutdown signal to UNO Q. |
| Perception-head connection | required | Mates head J1 (JST GH 14 for Rev A); routes ICD signals to MCU pins once frozen. |
| Trigger | required | MCU GPIO with hardware debounce (RC) + EXTI; wake-capable pin. |
| Rotary encoder + push | required | Needs a timer CH1/CH2 pair in encoder mode (e.g. TIM3: D8/PB4 + A2/PA6 candidate); keep out of the head's candidate pins. |
| Buttons (2–3) | required | MCU GPIO or I2C expander on bus A. |
| Service/debug | required | USB-C pass-through, UART console (JCTL SE4 is **1.8 V** — level-shift), test points on rails, SWD only if exposed by UNO Q. |
| Compact display adaptation | only if needed | Native 22-pin DSI via Media Carrier preferred; adapter only if the chosen panel needs a different FPC/power (e.g. 4-DSI-TOUCH-A 5 V input). |
| Media Carrier replacement/consolidation | deferred | Only if packaging forces it; would bring CSI/DSI routing onto our board — explicitly not Rev A. |

## Interfaces

- **To UNO Q:** standard UNO headers (JDIGITAL/JANALOG/Qwiic) for MCU-domain
  signals; power via 5 V pin or DC_IN. Do not use 1.8 V SoC GPIO without
  translation; never use CCI I2C or MI2S pins as GPIO.
- **To head:** [ICD_PERCEPTION_HEAD.md](ICD_PERCEPTION_HEAD.md) — the head's
  signal list is the contract; the host carrier adapts to it.
- **To display:** Media Carrier DSI0 (no change for Rev A).
- **To battery:** pack connector + SMBus (if smart pack) to MCU bus A or a
  dedicated bus — decide with the pack.

## Gates before layout

1. Display panel + driver path chosen (#41) and its 5 V load measured.
2. System power matrix measured (POWER_TREE_REV_A.md §4) → battery class.
3. Head ICD pins frozen (H6) and encoder/trigger pins allocated without conflicts.
4. Fusion gives the board envelope, connector positions and cable routes.
