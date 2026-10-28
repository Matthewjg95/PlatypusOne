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



## 2026-10-28 non-physical convergence — source audit and compile-only probe

**Decision:** issue #52's 4.0-inch portrait product target supersedes #41's earlier *preference* for the 3.5-inch H. It does not supersede the 3.5-inch prepared alternate, the proven 5-inch development fixture, or any physical acceptance gate. The 4.3-inch older ICN6211 panel is a larger landscape fallback, not the 4.3-DSI-TOUCH-A.

**Researched source facts, not measured results:**
- Arduino's official display page (last revised **2026-10-15**) still lists native UNO Q Media Carrier presets only for 5, 8 and 10.1 inches. The carrier uses a 22-pin DSI connector and enabling it disables USB-C DisplayPort: https://docs.arduino.cc/software/app-lab/configure/display/
- Waveshare SKU 34354 specifies **4.75–5.25 V**, **180 mA typical** (documentation says the supply must be capable of at least 180 mA), **480×800**. The required 5 V path is **not** supplied by the Media Carrier's DSI FFC: https://docs.waveshare.com/4-DSI-TOUCH-A
- Raspberry Pi rpi-6.18.y panel-waveshare-dsi-v2.c was inspected on **2026-10-28** at Git blob **c6cc003a35337971eef670587b200928ca1c1814**. The match waveshare,4.0-dsi-touch-a resolves to ws_panel_4_inch_a_desc → ws_panel_4_a_mode (480×800, 27.2 MHz) and ws_panel_4_a_init (**31 DCS commands and 3 delay entries**). The descriptor uses **2 DSI lanes**, RGB888, video/HSE/LPM/noncontinuous-clock flags: https://github.com/raspberrypi/linux/blob/rpi-6.18.y/drivers/gpu/drm/panel/panel-waveshare-dsi-v2.c
- The community dcuartielles/uno_q_dsi_displays project **has** a derived-panel workflow with a private compatible to avoid collision with built-in drivers. Its **4-DSI-TOUCH-C** result on an UNO Q is evidence that this *approach* can work, **not evidence that our 4-DSI-TOUCH-A works**: https://github.com/dcuartielles/uno_q_dsi_displays

**Software gate added:** tools/compact_display/preflight_4in.py follows the actual compatible → descriptor → mode/init chain, checks reviewed mode/flags, and fails closed if the source layout changes. Default mode is read-only. Optional --compile copies the source into a new build directory, invokes the community patcher with private compatible unoq,4-0-dsi-touch-a, rejects untrimmed compatibles, and runs make ... modules against **the running kernel's** headers. It does **not** install a .ko, edit a DTB/overlay, change boot configuration, or access hardware. Its test suite uses synthetic source and a fake make; those passes are **not** target-kernel compilation.

### Reproducible, non-installing probe

On a development machine or UNO Q, fetch the reviewed driver source (or copy it from another machine). Because the upstream branch is mutable, **check its Git blob hash** before claiming reproduction of this review:

~~~sh
curl -fL https://raw.githubusercontent.com/raspberrypi/linux/rpi-6.18.y/drivers/gpu/drm/panel/panel-waveshare-dsi-v2.c \
  -o /tmp/panel-waveshare-dsi-v2.c
git hash-object /tmp/panel-waveshare-dsi-v2.c
# Expected reviewed blob: c6cc003a35337971eef670587b200928ca1c1814
python3 tools/compact_display/preflight_4in.py --driver /tmp/panel-waveshare-dsi-v2.c
~~~

On the **UNO Q only**, with matching kernel headers under /lib/modules, make, and a checkout of dcuartielles/uno_q_dsi_displays already available, run **without sudo**:

~~~sh
python3 tools/compact_display/preflight_4in.py \
  --driver /tmp/panel-waveshare-dsi-v2.c \
  --patcher "$HOME/uno_q_dsi_displays/tools/patch-waveshare-panel.py" \
  --out "$HOME/platypus-4in-build-20261028" --compile
~~~

Archive result.json, patch.log, build.log, the source Git blob and kernel release. **No actual UNO Q compilation was performed in this sprint**; the board/kernel headers are not accessible to this automation. A compile pass would close only the **module compilation** subgate, not overlay, touch, power, CSI coexistence or physical fit.

**Unresolved integration details:** validate 5 V connector/polarity and shared ground, panel controller GPIO/overlay semantics, Goodix I²C address and axis mapping, boot recovery to the 5-inch fixture, and DSI/CSI concurrent use. Do not apply the existing 3.5-inch H installer to the 4-inch A; their supply, controller and initialization are different. Do not select/route/order a PCB on the strength of this source audit.
