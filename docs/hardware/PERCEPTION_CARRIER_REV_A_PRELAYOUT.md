# Rev A carrier pre-layout architecture

**Status:** pre-layout work may proceed; fabrication/layout remains gated by physical evidence.

## Decision

Do **not** turn every Platypus One interface into one large PCB yet.

Split the hardware into two design problems:

### A. Perception Head PCB

Rigidly belongs with the optical head and is intentionally independent of the
final display size.

Candidate responsibilities:
- mechanical datum for the selected IMX219 camera module;
- VL53L8CX-class ToF interface/integration;
- BMI270-class IMU;
- controlled white illumination driver(s);
- calibration/identity storage candidate;
- reset/interrupt/enable routing;
- rail filtering/decoupling;
- test points and current-measurement access;
- one documented low-speed/power interface back to the host.

The camera stays a replaceable module during Rev A testing and continues to
connect to the UNO Media Carrier CSI path. Do not route custom MIPI CSI on the
first perception-head PCB unless the camera bench-off creates a compelling
reason.

### B. Host / Power / Controls Carrier

This is mechanically coupled to the enclosure and display decision. Delay its
physical layout until the smaller-display path is selected.

Candidate responsibilities:
- battery/charge/system power distribution;
- trigger and encoder;
- perception-head power + MCU I2C/GPIO connection;
- service/debug access;
- smaller display adaptation if required;
- eventual replacement/consolidation of Media Carrier functions only if that
  is justified by packaging.

The existing UNO Media Carrier remains the multimedia reference platform while
camera and compact-display compatibility are proven.

## Work that may start now

### Schematic / interface work

Create the block diagram and pin-budget around named **functions**, not guessed
connector pins:

| Function | Current owner/path | Freeze state |
|---|---|---|
| RGB data | IMX219 module -> Media Carrier CSI0 | bench proof in progress |
| ToF data/control | VL53L8CX rig -> MCU-side I2C candidate | physical Lab evidence pending |
| IMU | BMI270-class -> MCU-side I2C/SPI candidate | part/interface candidate |
| illumination PWM/enable | MCU GPIO/PWM | function can be reserved now |
| calibration/board identity | local NVM candidate | architecture open |
| trigger/encoder | MCU GPIO | reserve function; exact pins later |
| display | Media Carrier DSI now; 3.5–4.0 in candidate next | NOT FROZEN |
| system power | USB-C bench supply now; battery later | NOT FROZEN |

It is safe now to:
- define connector roles and required signals;
- create rail tree alternatives;
- reserve ToF INT/LPn and IMU interrupt(s);
- define illumination switch/driver requirements;
- define test-point strategy;
- define calibration identity schema/storage requirements;
- create schematic sheets with explicit TBD net/pin labels;
- create mechanical keep-out/bounding models.

It is **not** safe now to:
- route/fabricate a board;
- copy the Media Carrier's high-speed MIPI implementation into a custom board;
- freeze the display connector;
- freeze a bare VL53L8CX implementation from datasheet assumptions;
- freeze camera optical position before the B0394/B0393/B0390 bench-off;
- size battery/regulators from unmeasured estimates.

## Physical gates that release layout

1. **Camera gate:** B0394/B0393/B0390 comparison identifies reference and
   product candidates, with preserved captures and focus/calibration behavior.
2. **ToF gate:** Platypus Lab produces replayable physical VL53L8CX evidence
   and identifies useful operating modes/failure behavior.
3. **Display gate:** an off-the-shelf 3.5–4.0 in display path is selected and
   electrically/software assessed against UNO Q/Media Carrier.
4. **Power gate:** measured UNO Q + display + camera baseline exists; sensor and
   illumination peak requirements are traceable.
5. **Mechanical gate:** Fusion supplies the optical-head datum and approximate
   folded-chassis envelope.

After gates 1–5, freeze connectors/rails/footprints and begin layout.

## Why split the boards

The sensor head wants optical rigidity and calibration repeatability. The host
carrier wants packaging, power and display flexibility. Keeping those concerns
separate lets the current camera/ToF work converge without waiting for the
smaller display and prevents a display change from invalidating the calibrated
sensor head.
