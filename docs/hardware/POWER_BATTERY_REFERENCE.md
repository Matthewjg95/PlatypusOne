# PlatypusOne Power & Battery Reference

Status: **Initial architecture reference — not a design freeze**  
Updated: 2026-09-22

## Design intent

PlatypusOne should be testable as a complete untethered system before the final enclosure and production battery are selected.

For Rev A development:

1. Use a known-good USB-C PD source / power bank for integrated bench and mobile testing.
2. Measure actual system power with the complete hardware stack.
3. Select the internal battery from measured idle, active, capture, compute, illumination, and peak loads.
4. Keep the final installed battery comfortably below airline lithium-ion limits.

## Air-travel requirement

**BAT-001 — The installed PlatypusOne battery shall be rated below 100 Wh and use a battery assembly with documented transport compliance (target: UN38.3). The design target is <=50 Wh unless measured runtime requirements justify more.**

Rationale:

- FAA passenger guidance allows rechargeable lithium-ion batteries from 0–100 Wh on passenger aircraft without the airline approval required for 101–160 Wh packs.
- Spare lithium batteries / power banks must be carried in carry-on baggage.
- Individual airlines and international carriers can impose stricter requirements.
- The battery Wh rating should be clearly marked on the pack/device.
- Targeting <=50 Wh gives substantial regulatory margin while keeping mass and enclosure volume reasonable.

FAA reference: https://www.faa.gov/hazmat/packsafe/airline-passengers-and-batteries

## Initial smart-battery candidates

These are **envelope and architecture candidates**, not approved BOM selections.

| Manufacturer P/N | Configuration | Nominal Voltage | Capacity | Energy | Max Discharge | Approx. Size | Weight | Role |
|---|---:|---:|---:|---:|---:|---|---:|---|
| **RRC2037** | 2S1P | 7.20 V | 3.35 Ah | 24.12 Wh | 4.50 A | 85.39 x 41.99 x 22.40 mm | 121 g | Lightweight lower-bound candidate |
| **RRC2130** | — | 7.20 V | 4.17 Ah | 30.02 Wh | 6.00 A | See vendor drawing | — | Flat-pack / mid-capacity candidate |
| **RRC2040** | 3S1P | 10.80 V | 3.35 Ah | 36.20 Wh | 4.00 A | 84.87 x 58.76 x 21.89 mm | 170 g | **Current reference favorite** |
| **RRC2054** | 4S1P | 14.40 V | >=3.40 Ah | >=48.96 Wh | 5.00 A | 85.10 x 77.40 x 22.40 mm | 230 g | Long-runtime / upper Rev A target |
| **RRC2057** | 2S2P | 7.20 V | 6.90 Ah | 49.70 Wh | 9.50 A | See vendor drawing | — | High-current ~50 Wh comparison |
| **RRC2040-2** | 3S2P | 10.80 V | >=6.80 Ah | >=73.44 Wh | 10.00 A | See vendor drawing | — | Runtime reference; likely oversized for final handheld |

RRC standard battery family reference:  
https://www.rrc-ps.com/en/battery-packs/standard-battery-packs

Specific references:

- RRC2037: https://www.rrc-ps.com/en/battery-packs/standard-battery-packs/products/rrc2037/
- RRC2040: https://www.rrc-ps.com/en/battery-packs/standard-battery-packs/products/rrc2040/
- RRC2054: https://www.rrc-ps.com/en/battery-packs/standard-battery-packs/products/RRC2054
- RRC2057: https://www.rrc-ps.com/en/battery-packs/standard-battery-packs/products/rrc2057/
- RRC2040-2: https://www.rrc-ps.com/en/battery-packs/standard-battery-packs/products/RRC2040-2

### Why RRC is useful for the first comparison

RRC's standard packs provide a useful industrial reference because the family includes smart BMS functionality, SMBus communication, protection features, state-of-charge support, and published compliance / transportation documentation. The RRC2037, RRC2040 and RRC2054 product pages currently list UN38.3 among their compliance information.

This does **not** mean PlatypusOne is committed to RRC. These packs are currently being used to establish realistic electrical, mass, thermal, and CAD envelopes.

## Current preferred architecture candidate

### RRC2040 reference envelope

Current first-choice envelope for modeling:

- 10.80 V nominal
- 3.35 Ah
- 36.20 Wh
- 4.00 A maximum discharge
- 84.87 x 58.76 x 21.89 mm
- 170 g
- 3S1P Li-ion
- SMBus smart-battery interface
- Published UN38.3 compliance

Why it is interesting:

- Large enough to represent meaningful handheld runtime.
- Well below the 100 Wh passenger-aircraft threshold.
- 3S nominal voltage is compatible in principle with a system architecture feeding the UNO Q VIN range, subject to final power-path / charger design validation.
- Compact enough to begin meaningful grip/body packaging studies.

Do **not** mechanically freeze the enclosure around RRC2040 until full-system power measurements have been collected.

## Development power meter

### Recommended: Plugable USBC-VAMETER3

Manufacturer P/N: **USBC-VAMETER3**

Key characteristics:

- Inline USB-C voltage / current / wattage measurement
- USB PD 3.1 EPR support
- Up to 240 W
- 4.5–50 V operating measurement range
- Bidirectional
- USB data passthrough
- OLED display

Reference: https://plugable.com/products/usbc-vameter3

This meter is intended for fast characterization, not as a replacement for laboratory-grade power instrumentation.

## Power characterization matrix

When the prototype stack is available, record at minimum:

| Configuration | Boot Peak | Idle | Live Preview | Capture | Heavy Compute | Notes |
|---|---:|---:|---:|---:|---:|---|
| UNO Q only | TBD | TBD | N/A | N/A | TBD | Baseline |
| + Media Carrier | TBD | TBD | N/A | N/A | TBD | Carrier overhead |
| + Camera | TBD | TBD | TBD | TBD | TBD | USB webcam first, then CSI |
| + Display | TBD | TBD | TBD | TBD | TBD | Use final/Santop sample when available |
| + ToF | TBD | TBD | TBD | TBD | TBD | |
| + Radar | TBD | TBD | TBD | TBD | TBD | |
| + Illumination | TBD | TBD | TBD | TBD | TBD | Test max brightness |
| Full stack | TBD | TBD | TBD | TBD | TBD | Battery sizing input |

Also record:

- USB-C negotiated voltage/current mode
- average power during a 15–30 minute realistic workflow
- transient peaks
- thermal behavior
- shutdown / brownout behavior
- runtime from a known-capacity development power bank

## Battery sizing method

After full-stack measurements:

1. Determine realistic average active power.
2. Choose desired continuous runtime.
3. Calculate ideal energy: **Wh = average W x runtime h**.
4. Add conversion loss, aging, reserve, cold-temperature, and peak-current margin.
5. Compare resulting requirement against the 24, 30, 36, and ~50 Wh candidate classes.
6. Revisit enclosure mass distribution before final selection.

Example only:

- 8 W average x 3 h = 24 Wh ideal
- 12 W average x 3 h = 36 Wh ideal

Actual pack selection must use measured system data.

## Production architecture questions still open

- Integrated smart battery vs custom protected pack
- Battery -> system power manager -> UNO Q VIN vs USB-C PD-internal architecture
- USB-C charge controller / PD sink implementation
- Whether SMBus battery telemetry is exposed to PlatypusOS
- Field-removable/service-removable vs permanently installed battery
- Battery placement in grip vs main body
- Thermal isolation from compute and display
- Charge-while-operating behavior
- Shipping/service strategy
- Final certification requirements for a commercial unit

## Near-term decision gate

**Do not select the final internal battery yet.**

Select it after:

- UNO Q + camera acquisition pipeline is characterized
- Media Carrier is installed
- development display / Santop display load is known
- ToF and radar current draw is measured
- illumination load is measured
- full-stack peak and average power are logged

At that point, choose the smallest battery class that meets runtime and current requirements with adequate margin.
