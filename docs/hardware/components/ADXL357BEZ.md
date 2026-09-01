# ADXL357BEZ Component Bring-Up Dossier

Status: **RECEIVED**
Disposition: **DE-RISK — precision vibration/tilt module; candidate for electromechanical diagnostics**
Quantity: **2**
Package received: ESD-protective packaging; photos and label details pending
Evidence date: 2026-09-01

## Why it matters to PlatypusOne

The ADXL357 is unusually well aligned with PlatypusOne's engineering-instrument role.
It provides low-noise, low-drift three-axis acceleration data, a 20-bit ADC,
programmable filtering, synchronous sampling, FIFO, interrupts, and an integrated
temperature sensor. That makes it useful for:

- machine vibration and condition monitoring;
- motor/actuator characterization alongside command, encoder, and current data;
- precision tilt and platform alignment;
- structural response experiments;
- documenting repeatable before/after measurements.

This is a much stronger fit than treating it as another generic orientation sensor.
It does **not** replace a BNO055/BMI270-class IMU when gyro data, heading, or fused
orientation is required. Its role is precision acceleration and vibration.

The best architecture is probably a small, mechanically deliberate sensor daughterboard
or probe rather than placement on the noisy, flexing main carrier. One sample can become
the development article and the second can support a corrected layout or matched
reference experiment.

## Verified identity and package

| Field | Value |
|---|---|
| Manufacturer part | ADXL357BEZ |
| Device | Digital-output, low-noise, low-drift, 3-axis MEMS accelerometer |
| Package | 14-terminal ceramic leadless chip carrier (LCC), package option E-14-1 |
| Nominal package class | 6 mm × 6 mm; use the current ADI footprint figure and outline |
| Measurement ranges | User-selectable ±10 g, ±20 g, ±40 g |
| Resolution | 20-bit ADC |
| Output interfaces | SPI or limited I2C |
| Digital filter passband | Programmable; approximately 0.977 Hz to 1 kHz |
| Operating temperature | -40°C to +125°C |
| Storage temperature | -55°C to +150°C |
| Measurement current | Approximately 200 µA with internal regulators enabled |
| Standby current | Approximately 21 µA with internal regulators enabled |
| Moisture sensitivity | **Verify from the received label before assembly**; related authorized-distributor listings show MSL 1, but the exact received packaging is controlling evidence |

## PlatypusOne disposition

**DE-RISK now; likely V2 core diagnostic sensor.**

The first carrier should reserve an interface and mounting strategy without necessarily
placing the bare MEMS package on the main PCB. A replaceable sensor daughterboard has
several advantages:

- isolates the precision sensor from processor/display heat and board flex;
- allows rigid attachment close to the machine under test;
- permits orientation changes and calibrated mounting;
- supports a Rev-A/Rev-B comparison using the two received samples;
- makes replacement possible after assembly or handling mistakes.

For the PlatypusTail electromechanical learning fixture, this sensor supplies the
vibration channel in the synchronized dataset:

`command + encoder + current + acceleration + temperature → diagnosis`.

## Supplies and power sequencing

Simplest Rev-A configuration:

- VSUPPLY: 2.25 V to 3.6 V, nominal 3.3 V;
- VDDIO: 2.25 V to 3.6 V, selected to match the controller logic domain;
- use the internal LDO regulators for V1P8ANA and V1P8DIG;
- provide the datasheet-recommended local decoupling at every supply pin;
- bring the device up in standby, configure interface/filter/range, then enter
  measurement mode.

Do not drive V1P8ANA or V1P8DIG externally unless deliberately bypassing the internal
regulators. The bypass arrangement changes how VSUPPLY is connected and must follow the
datasheet exactly.

All digital pins are limited relative to VDDIO. Avoid applying SPI/I2C signals while
VDDIO is absent because that can back-power or overstress the interface.

## Interface choice

**SPI is preferred for vibration characterization.**

It provides deterministic high-rate capture, avoids the ADXL357's limited-I2C caveats,
and is easier to combine with DATA_RDY or external synchronization. Reserve:

- CS;
- SCLK;
- MOSI;
- MISO;
- DATA_RDY or one interrupt;
- optional second interrupt;
- ground and 3.3 V.

I2C remains useful for a low-rate tilt demonstration but should not be the default for
the full vibration/condition-monitoring path.

## Important firmware behavior

- Verify manufacturer and part-ID registers at startup.
- Run the built-in electromechanical self-test and record its result.
- Configure range, ODR, low-pass/high-pass filtering, FIFO, and interrupt routing
  explicitly; do not rely on undocumented defaults.
- Timestamp DATA_RDY edges in the real-time controller so acceleration can align with
  current and encoder data.
- Monitor FIFO overrun and data-loss conditions.
- Record temperature with measurements because offset varies with temperature.
- Avoid unnecessary software resets. The datasheet documents a rare ADXL357 shadow-
  register load race after software reset. Save registers 0x50–0x54 after power-up and
  compare them after a software reset; repeat reset if they do not match.

## Mechanical and layout requirements

Mechanical design is part of the measurement chain.

- Use ADI's current footprint pattern; do not approximate the LCC lands.
- Place decoupling next to the relevant pins with a quiet return path.
- Keep the sensor away from regulators, high-current switching loops, display heat,
  board edges, mounting screws, connectors, and areas that bend during use.
- Use a small, stiff, symmetric sensor PCB.
- Put mounting features close enough to create a repeatable mechanical path without
  loading or twisting the package.
- Define and mark the sensor axes and board datum on silkscreen and CAD.
- Keep the same orientation between calibration, test, and installed use.
- Avoid potting, adhesive, enclosure pressure, or uneven fastening until their induced
  bias is characterized.

ADI notes that package/PCB stress can create acceleration offset; its precision-sensing
guidance reports that compressive or tensile stress can create offsets large enough to
ruin a high-accuracy tilt result. The daughterboard must therefore be mechanically
treated like a sensor, not generic electronics.

## Handling at home

Use the same controlled work area defined in
[Carrier Board Development Methodology](../CARRIER_BOARD_DEVELOPMENT_METHOD.md):

- leave each part in its original ESD packaging until the grounded mat and wrist strap
  are ready;
- open only on the grounded static-dissipative mat;
- handle the package by its sides with ESD-safe tweezers;
- never touch, scrape, or probe the termination surfaces;
- retain original tray/pocket orientation and labeling;
- avoid dropping the loose ceramic package or allowing tools/components to strike it;
- return unused parts immediately to static-shielding packaging;
- record the exact MSL label and opening time when photos are taken.

The datasheet explicitly identifies the device as ESD sensitive. Its 10,000 g,
0.1 ms absolute shock rating is a survivability limit, not permission for casual
handling and not a guarantee that calibration remains unchanged after abuse.

## Assembly implications

The 14-terminal LCC is substantially more approachable than the MAX78002 BGA, but the
precision MEMS performance makes assembly and mechanical design consequential.

Preferred first build:

1. professionally assemble a dedicated sensor daughterboard;
2. use the official land pattern and lead-free reflow profile;
3. visually inspect all perimeter joints;
4. power up in standby and check current;
5. verify device IDs and self-test;
6. characterize noise and bias before mounting;
7. mount to the test fixture and repeat the characterization;
8. use the second sample for Rev B or an A/B mounting experiment.

A skilled hot-air/reflow home assembly may be possible, but it is not the preferred use
of two expensive precision samples. Outsourced assembly gives us a cleaner baseline.

## Likely kill or performance-degradation mechanisms

- ESD before or during assembly.
- Supply or digital-interface overvoltage.
- Back-powering through SPI/I2C pins.
- Incorrect LDO-bypass wiring.
- Wrong LCC footprint or poor solder wetting.
- PCB strain, enclosure pressure, mounting torque, or thermal gradient creating bias.
- Conducting precision calibration on a soft or moving surface.
- Aliasing due to an incorrect ODR/filter combination.
- FIFO overflow or unsynchronized polling.
- Treating ±10/20/40 g range as bandwidth or survivability.
- Assuming a successful self-test proves calibration accuracy.
- Shadow-register corruption after a software reset without the documented check.

## First bring-up experiment

### Phase A — electrical proof

- 3.3 V supply with current limiting.
- SPI identity-register read.
- Read temperature and all three axes.
- Confirm approximately 1 g vector magnitude while stationary.
- Rotate through +X/-X, +Y/-Y, and +Z/-Z.
- Execute and log self-test.

### Phase B — precision baseline

For each sample:

- capture 60 seconds stationary at a defined ODR and filter setting;
- calculate mean, standard deviation, vector magnitude, and temperature;
- repeat after rotating to all six faces;
- compare the two devices and record unit-to-unit offset.

### Phase C — PlatypusTail/PlatypusOne diagnostic proof

Mount one daughterboard to the motor test fixture and synchronously record:

- motor command;
- encoder speed;
- current;
- XYZ acceleration;
- sensor temperature.

Run baseline, added imbalance, looseness, and load-change conditions. Determine whether
frequency-domain and time-domain features distinguish the deliberate faults.

**Pass criterion:** repeatable detection of at least two deliberate mechanical
conditions while maintaining a traceable raw dataset and test setup.

## Open questions / schematic blockers

- Received packaging label, lot/date code, and MSL.
- Exact controller: UNO Q MCU bridge, RA6T3 test controller, or dedicated sensor MCU.
- Required sample rate and anti-alias strategy for the target motor.
- Daughterboard connector and cable length.
- Mechanical coupling method and allowable mounting torque.
- Whether external synchronization is required in Rev A.
- Whether one sample should remain sealed as a replacement/reference.
- Assembly house capability for the LCC and inspection method.

## Sources

- [Analog Devices ADXL357 product page](https://www.analog.com/en/products/adxl357.html)
- [ADXL356/ADXL357/ADXL357B datasheet, Rev. D](https://www.analog.com/media/en/technical-documentation/data-sheets/adxl356-357-357b.pdf)
- [ADI EVAL-ADXL35X resources](https://www.analog.com/en/products/adxl357.html#product-evaluationkit)
- [DigiKey ADXL357BEZ product record](https://www.digikey.com/en/products/detail/analog-devices-inc/ADXL357BEZ/6665426)
- [ADI: Managing Stress and Strain in High-Precision Tilt Sensing](https://www.analog.com/en/resources/technical-articles/managing-stress-and-strain-to-get-the-best-perf-in-hi-prec-tilt-angle-sensing.html)
