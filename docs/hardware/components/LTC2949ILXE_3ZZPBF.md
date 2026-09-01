# LTC2949ILXE#3ZZPBF Component Bring-Up Dossier

Status: **RECEIVED**
Disposition: **DE-RISK — switched power-characterization module; future robotics/high-power monitor**
Quantity: **2**
Package received: ESD/dry-pack details pending photos
Evidence date: 2026-09-01

## Why it matters to PlatypusOne

The LTC2949 is not merely a battery gauge. It simultaneously measures current and
voltage, calculates power, and continuously accumulates charge and energy. It has two
precision current-sense channels, up to 12 buffered auxiliary voltage inputs, threshold
registers, open-wire detection, temperature measurement support, SPI/isoSPI, and
calibration-factor storage through an external I2C EEPROM.

That creates two high-value uses:

1. **PlatypusOne power-characterization pod** — quantify the energy cost of displays,
   cameras, radios, inference, motors, and operating modes during development.
2. **PlatypusTail/robotics power monitor** — synchronize electrical power with encoder
   and ADXL357 vibration data to diagnose load, friction, imbalance, binding, and faults.

It should **not** automatically become PlatypusOne's always-on internal battery gauge.
The device requires 4.5 V to 14 V supplies and typically draws approximately 16–20 mA
while measuring, which is excessive for continuous monitoring in a small single-cell
handheld. It earns its place first as a switched, separately testable instrumentation
module. A simpler low-quiescent-current gauge remains appropriate for product battery
state.

## Verified identity and package

| Field | Value |
|---|---|
| Manufacturer part | LTC2949ILXE#3ZZPBF |
| Function | Current, voltage, power, charge, energy, and auxiliary-input monitor |
| Package | 48-lead 7 mm × 7 mm plastic eLQFP with exposed pad |
| Exposed pad | Pin 49; connect to AGND |
| Temperature grade | I grade, -40°C to +85°C ambient |
| Automotive suffix | #3ZZ controlled-manufacturing automotive version |
| Moisture sensitivity | MSL 3, 168-hour floor life |
| Main supply | AVCC and DVCC: 4.5 V to 14 V |
| Measurement current | Approximately 16 mA typical, 20 mA maximum |
| Sleep current | Operating-mode/configuration dependent; verify the intended sleep state from the full datasheet |
| Current ADC | 20-bit filtered mode; <1 µV offset specification |
| Current input range | ±110 mV specified differential range |
| Battery/power voltage input | ±4.8 V specified differential range |
| Interface | Direct SPI or transformer-coupled isoSPI |
| SPI logic supply | IOVCC: 1.8 V to 4.5 V |

## PlatypusOne disposition

**DE-RISK as an external or switched instrumentation module.**

The first experiment should not route the system battery through an unproven main
carrier. Build a protected module with clearly separated domains:

- source/load terminals;
- one or two Kelvin-connected current shunts;
- fused/current-limited input;
- LTC2949 analog section;
- SPI or isoSPI communications;
- calibration EEPROM;
- isolated or otherwise safe host boundary;
- test points for every supply and sense input.

This module can become a reusable lab instrument shared by PlatypusOne, PlatypusTail,
energy-harvesting tests, actuator development, and batteryless Scout-module work.

## What it adds beyond a simple current monitor

- simultaneous current and pack/link voltage sampling;
- hardware power calculation;
- continuous charge and energy integration;
- two current-sense channels;
- programmable overcurrent comparators;
- high- and low-side sensing;
- 12 buffered auxiliary inputs for thermistors, dividers, contactor states, or other
  voltages;
- temperature compensation support for the sense resistor;
- open-wire detection;
- isolated daisy-chain communications through isoSPI;
- synchronous operation with LTC68xx/ADBMS68xx cell monitors.

The real advantage is synchronized electrical evidence, not a prettier battery
percentage.

## Supply and interface architecture

### Supply

AVCC and DVCC must stay closely matched and both operate from 4.5 V to 14 V. For a
PlatypusOne bench module, a regulated, current-limited 5 V supply is the simplest
starting point. Do not assume a depleted single Li-ion cell can power the device
directly; it falls below the 4.5 V undervoltage threshold.

The device produces internal regulated BYP1 and BYP2 rails. These require the exact
datasheet capacitors and must not be treated as general-purpose module power outputs.

### Host interface

For a short, same-board development connection, direct SPI is simplest. Set IOVCC to
the controller's logic voltage and verify that every SPI pin remains within that domain.

For a remote, noisy, high-common-mode, or high-energy measurement pod, isoSPI is the
architecturally stronger option. It requires the correct pulse transformer and bias/
termination network. The official DC2732A design is the reference.

The isolated communications interface does **not** by itself make the complete module
safe. Power, shunt placement, creepage, clearance, fusing, connectors, enclosure, and
test method determine the actual isolation and hazard rating.

## Current-sense design

The shunt is part of the measurement system.

- Select the shunt from peak current, allowable burden voltage, power dissipation, and
  desired resolution.
- Keep the expected differential voltage inside the specified ±110 mV range.
- Use true Kelvin connections from the shunt sense points to the input filter.
- Route both sense traces as a matched pair away from switching nodes.
- Place the input filter exactly as supported by ADI's reference design.
- Keep load current out of the sense-return traces and analog ground.
- Account for shunt temperature coefficient and self-heating.
- Provide a safe method to bypass/remove the module during early system bring-up.

For a first low-voltage experiment, choose a current-limited USB or bench-supply load,
not a large battery or motor pack.

## Voltage and auxiliary inputs

The direct battery/power ADC differential range is approximately ±4.8 V; high-voltage
stack measurements use precision resistor dividers and the reference architecture.
Auxiliary V1–V12 inputs can measure divided voltages, thermistors, link voltage,
isolation-related signals, or contactor states.

Every input requires a documented maximum pin voltage and fault case. The presence of a
high-voltage application diagram is not permission to improvise divider networks on
mains, EV, or other hazardous-energy systems.

## Layout, grounding, and assembly

- Use ADI's exact eLQFP land pattern.
- Connect the exposed pad to AGND with the recommended copper and via structure.
- Keep AGND and DGND topology consistent with the reference design; do not let high load
  current flow through either.
- Place current-input filters and shunt connectors before routing digital signals.
- Keep isoSPI transformer and network compact and isolated from sensitive analog
  inputs.
- Separate high-energy terminal spacing from logic/debug connectors.
- Add labeled test points for AVCC, DVCC, IOVCC, BYP1, BYP2, VREF, AGND, and DGND.
- Use 0 Ω links or jumpers to isolate power and analog blocks during bring-up.
- Confirm exposed-pad paste coverage, thermal vias, coplanarity, and inspection with
  the assembler.

The perimeter leads make this package easier to inspect and rework than the MAX78002
BGA, but the exposed pad still favors controlled stencil/reflow assembly.

## Handling at home

LTC2949ILXE#3ZZPBF is **MSL 3**. Leave both samples sealed until the PCB and assembly
window are ready.

- Store in the original moisture-barrier/static-shielding packaging.
- Open only at the grounded ESD mat while wearing the wrist strap.
- Photograph the MSL label, HIC, desiccant, lot/date code, quantity, and tray position
  before removing anything.
- Log the opening date and time; MSL 3 floor life is 168 hours under the rated ambient
  conditions before reflow.
- Handle only by the package edges with ESD-safe tweezers.
- Do not touch or bend the leads; keep the part in its formed tray pocket.
- If the floor-life window is exceeded, have the assembler apply its controlled
  J-STD-033 bake/dry-pack process. Do not use a kitchen oven.
- Preserve one sample sealed until the first design and assembly review close.

## High-energy safety boundary

The device is designed for high-voltage battery packs, but our first experiments are
restricted to extra-low voltage and current-limited sources.

Do not connect a custom Rev-A board to:

- mains;
- an EV or hybrid pack;
- a large unprotected lithium pack;
- any source whose prospective fault current exceeds the test fixture's protection;
- a high-side node without a reviewed isolation and probing plan.

Before progressing beyond low voltage, the design needs documented isolation ratings,
creepage/clearance, fuse selection, connector touch protection, discharge behavior,
test equipment ratings, and a formal safe-work procedure.

## Firmware and data considerations

- Validate device identity and PEC-protected communications.
- Explicitly configure current channels, voltage channel, ADC modes, thresholds, and
  accumulation.
- Use an external accurate clock if the charge/energy accuracy experiment requires it;
  the datasheet specifies worse accumulated accuracy with the internal clock.
- Store board calibration coefficients in the supported external EEPROM.
- Record raw codes as well as converted engineering units.
- Detect counter rollover, communication errors, open-wire results, and invalid state.
- Timestamp LTC2949 records against ADXL357, encoder, and command data.
- Preserve shunt value, tolerance, temperature coefficient, divider values, firmware
  revision, and calibration date with every dataset.

## First bring-up experiment

### Phase A — protected low-voltage board proof

1. Inspect the assembled module and verify all supply-to-ground resistances.
2. Power only AVCC/DVCC from a current-limited 5 V supply.
3. Verify BYP1, BYP2, VREF, and quiescent current.
4. Bring up direct SPI at a conservative clock rate.
5. Read identity/status registers and run communication error tests.
6. Confirm zero-current and shorted-input offsets before installing a shunt.

### Phase B — known-load characterization

Use a fused/current-limited 5 V source and resistor/electronic load:

- measure at several known currents with a calibrated DMM reference;
- compare LTC2949 current, voltage, and power results;
- run a timed load and compare accumulated charge/energy against calculation;
- repeat after thermal equilibrium;
- document offset, gain error, noise, drift, and uncertainty.

### Phase C — electromechanical diagnostic loop

On a safe low-voltage motor fixture, synchronously record:

- command;
- encoder speed;
- LTC2949 current, voltage, power, charge, and energy;
- ADXL357 XYZ acceleration and temperature.

Introduce bounded load, imbalance, and mechanical resistance changes. Determine whether
electrical and vibration evidence together distinguish faults more reliably than either
sensor alone.

**Pass criterion:** calibrated low-voltage current/power measurement within the
datasheet-supported error budget and at least one repeatable diagnostic improvement
from combining electrical and vibration data.

## Likely kill or failure mechanisms

- ESD or moisture exposure before reflow.
- Bent LQFP leads or poor exposed-pad soldering.
- AVCC/DVCC below operating range or separated beyond allowed mismatch.
- Overvoltage on IOVCC, SPI, I2C, or analog pins.
- Missing/incorrect BYP or reference capacitors.
- Shunt sense traces carrying load current instead of true Kelvin sensing.
- Excessive shunt burden voltage or power dissipation.
- Ground loops between the monitored power path and host.
- Assuming isoSPI automatically provides a complete safety isolation barrier.
- Incorrect high-voltage divider, spacing, or probe method.
- Counter/clock/calibration errors producing plausible but wrong energy totals.
- Starting with a dangerous battery source before low-voltage validation.

## Open questions / schematic blockers

- Received label, lot/date code, HIC/desiccant condition, and current storage state.
- Exact first-use current and voltage range.
- Shunt value, tolerance, power rating, and temperature coefficient.
- Direct SPI versus isoSPI for the first module.
- Isolation and power architecture for future robot/high-voltage use.
- Required calibration reference instruments.
- Whether one channel monitors system input while the second monitors a subsystem.
- Connector family, current rating, polarity keying, and fuse strategy.
- Assembly-house requirements for the exposed pad and MSL-3 parts.
- Whether the newer ADBMS2950/ADBMS2960 should influence a later product revision;
  the received LTC2949 remains fully useful for the learning and validation platform.

## Sources

- [Analog Devices LTC2949 product page](https://www.analog.com/en/products/ltc2949.html)
- [LTC2949 datasheet, Rev. A](https://www.analog.com/media/en/technical-documentation/data-sheets/ltc2949.pdf)
- [ADI DC2732A demonstration-board resources](https://www.analog.com/en/products/ltc2949.html#product-evaluationkit)
- [ADI reliability report for LTC2949ILXE#3ZZPBF](https://www.analog.com/en/reliability/Product/GeneratePdf?datasheetLink=https://www.analog.com/en/LTC2949/datasheet&modelName=LTC2949ILXE%233ZZPBF&modelType=automotive)
- [DigiKey LTC2949ILXE#3ZZPBF product record](https://www.digikey.com/en/products/detail/analog-devices-inc/LTC2949ILXE-3ZZPBF/11312126)
