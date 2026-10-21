# Four-inch display integration gate — UNO Q / Media Carrier

Research date: 2026-10-21. **No compact-panel physical compatibility has been demonstrated.** This is a design and software gate, not a purchasing or enclosure release.

## Decision reconciliation

Issue #41 selected the 3.5-inch Waveshare DSI LCD (H), SKU 33087, provisionally on 2026-10-14. Newer issue #52 (2026-10-18) requests a **4-inch portrait product target**. Treat the 4-inch as the current *Fusion envelope-study target*, while preserving the 3.5-inch as a software-prepared alternate. Neither is a proven replacement for the existing working five-inch display.

Do not conflate these three product families:

| Candidate | Hardware | UNO Q state |
| --- | --- | --- |
| Waveshare **4-DSI-TOUCH-A**, SKU 34354 | 480x800 portrait; 108.30x65.10 mm outer, 86.40x51.84 mm active; **5 V**, nominal 180 mA; two-lane DSI, ST7701S, GT911 | **Not verified**. Requires a driver port, DSI initialization, a separate 5 V supply, and an overlay. |
| Waveshare **3.5inch DSI LCD (H)**, SKU 33087 | 480x800 portrait; 3.3 V from display ribbon, GT911; self-initializing in vendor overlay | Overlay/installer and host tests prepared in `tools/compact_display/`; **not physically tested**. |
| Waveshare **4.3inch DSI LCD** (older 800x480 ICN6211 family) | Larger landscape panel; same upstream panel definition as our 5inch DSI LCD | Third-party UNO Q proof, **not** proof on our specific unit. Different from **4.3-DSI-TOUCH-A** (ST7701S). |

Arduino's native UNO Q Media Carrier presets are for 5, 8 and 10.1-inch displays. A Raspberry Pi-compatible connector or Linux driver is not proof of Qualcomm MSM DSI operation.

## Source evidence

- Waveshare SKU and 5 V supply: https://docs.waveshare.com/4-DSI-TOUCH-A
- Waveshare panel controller and dimensions: https://www.waveshare.com/4-dsi-touch-a.htm
- Arduino display presets (revised 2026-10-08): https://docs.arduino.cc/software/app-lab/configure/display
- Raspberry Pi downstream source: https://github.com/raspberrypi/linux/blob/rpi-6.18.y/drivers/gpu/drm/panel/panel-waveshare-dsi-v2.c
- Third-party UNO Q 4.3/5-inch verification: https://github.com/dcuartielles/uno_q_dsi_displays

The Raspberry Pi driver includes the `waveshare,4.0-dsi-touch-a` match, a 480x800 27.2 MHz mode, a two-lane RGB888 descriptor and a vendor init sequence. That is **porting source**, not a compiled UNO Q driver or an observed screen.

