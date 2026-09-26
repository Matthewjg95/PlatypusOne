# STEP Files

Mechanical models used to package Platypus One.

## Current assets

- `arduino_q.step` — Arduino UNO Q board model. Arduino publishes official STEP files for the UNO Q.

## Initial component selections / sources

- Arduino UNO Q — core compute board (required by contest).
- 5-inch DSI display + UNO Media Carrier envelope — see the [panel candidates and validation gates](../../docs/hardware/DSI_PANEL_CANDIDATES.md); final mechanical fit remains conditional.
- Flat Li-ion battery — preferred packaging direction; exact cell not yet selected.
- Rotary encoder — primary physical navigation control; exact part not yet selected.

## Source links

- Arduino UNO Q hardware page: https://docs.arduino.cc/hardware/uno-q
- Arduino UNO Q official STEP archive: https://github.com/arduino/docs-content/blob/main/content/hardware/02.uno/boards/uno-q/downloads/ABX00162-step.zip
- Current display direction: [DSI panel candidates](../../docs/hardware/DSI_PANEL_CANDIDATES.md).

> Do not treat provisional envelope models as production CAD. Replace them with vendor STEP files once exact part numbers are frozen.
