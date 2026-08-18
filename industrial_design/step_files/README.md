# STEP Files

Mechanical models used to package Platypus One.

## Current assets

- `CAD/arduino_q.step` — Arduino UNO Q model already present in the repository. Arduino publishes official STEP files for the UNO Q.

## Initial component selections / sources

- Arduino UNO Q — core compute board (required by contest).
- 5-inch display envelope — current packaging target; final display part number still under evaluation.
- Grove Doppler Radar (BGT24LTR11) — advanced motion/vibration sensing candidate.
- Flat Li-ion battery — preferred packaging direction; exact cell not yet selected.
- Rotary encoder — primary physical navigation control; exact part not yet selected.

## Source links

- Arduino UNO Q hardware page: https://docs.arduino.cc/hardware/uno-q
- Arduino UNO Q official STEP archive: https://github.com/arduino/docs-content/blob/main/content/hardware/02.uno/boards/uno-q/downloads/ABX00162-step.zip
- Seeed Grove Doppler Radar wiki: https://wiki.seeedstudio.com/Grove-Doppler-Radar/
- Waveshare 5inch HDMI LCD (H) V4 reference: https://www.waveshare.com/wiki/5inch_HDMI_LCD_%28H%29_V4

> Do not treat provisional envelope models as production CAD. Replace them with vendor STEP files once exact part numbers are frozen.
