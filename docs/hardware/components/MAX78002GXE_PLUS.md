# MAX78002GXE+ Component Bring-Up Dossier

Status: **RECEIVED — unopened handling recommended until assembly plan exists**
Disposition: **RESEARCH / possible V2 edge-AI coprocessor**
Quantity: **2**
Package received: ESD-protective bag; record moisture-barrier label, desiccant, and
humidity-indicator condition before opening
Evidence date: 2026-09-01

## Why it matters to PlatypusOne

The MAX78002 is a serious low-power vision/inference candidate: it combines a
120 MHz Arm Cortex-M4F, a 60 MHz RISC-V coprocessor, and a CNN accelerator capable of
VGA processing at up to 30 fps. It also exposes MIPI CSI-2 and a parallel camera
interface. That maps directly to future always-on inspection, defect classification,
and Engineering Scout workloads.

It is **not a Rev-A contest-core commitment**. PlatypusOne already centers the UNO Q
for the bounded build, and this sample is a bare 144-ball BGA requiring a complete
power/clock/debug/boot design and professional assembly. The best near-term use is to
study the official EV kit and AI toolchain, then decide whether a future module earns
its power, board area, firmware, and integration cost.

## Verified identity and package

| Field | Value |
|---|---|
| Manufacturer part | MAX78002GXE+ |
| Device | Low-power AI microcontroller with CNN accelerator |
| Package | 144 CSBGA, 12 mm × 12 mm, 0.8 mm pitch |
| Package code | X14422+2C |
| Outline / land pattern | 21-0163 / 90-0185 |
| Operating temperature | -40°C to +105°C |
| Storage temperature | -65°C to +125°C |
| Datasheet soldering maximum | +260°C; this is an absolute package limit, not a home reflow recipe |
| Moisture sensitivity | MSL 3, 168-hour floor life at ≤30°C / 60% RH (distributor attribute; package label governs) |

## Functional summary

- Arm Cortex-M4F up to 120 MHz with FPU
- RISC-V coprocessor up to 60 MHz
- 2.5 MB flash, 384 KB SRAM, and 64 KB ROM
- CNN accelerator with 2 MB 8-bit weight capacity and 1.3 MB data memory
- MIPI CSI-2 two-lane and 12-bit parallel camera interfaces
- USB 2.0 high-speed, SD/SDIO/eMMC, I2S, SPI, I2C, UART, ADC, timers/PWM
- Integrated SIMO power management with dynamic voltage scaling
- Secure boot option, AES acceleration, TRNG, and unique ID

## Supplies, sequencing, reset, and clocks

This is not a single-rail drop-in MCU.

Verified operating domains include:

| Domain | Range / relationship |
|---|---|
| VBAT, VREGI, VDDIOH | 2.85–3.6 V; datasheet requires these connected together at board level |
| VDDA, VDDIO | 1.71–1.89 V; datasheet requires these connected together at board level |
| VCOREA, VCOREB | 0.9–1.21 V |
| Active CNN and CNN-RAM domains | 0.99–1.21 V and must match VCOREA when enabled |
| MIPI CSI supply | Dedicated 2.5 V domain; full requirements need extraction from the reference design |

The integrated SIMO architecture, inductor nodes, output capacitors, CNN rail enables,
USB rails, reset, clocking, and boot configuration must be copied from and checked
against the datasheet and EV-kit schematic. Do not create a schematic from this summary.

## Interfaces and likely PlatypusOne boundary

Candidate future interfaces to the UNO Q:

1. USB device link — potentially cleanest subsystem boundary if supported by the
   selected firmware stack.
2. SPI + interrupt/control GPIO — lower overhead but creates voltage-domain and
   protocol work.
3. UART — useful for first communication proof, not high-bandwidth vision transport.
4. Camera directly into MAX78002 — gives the accelerator ownership of capture and sends
   only results or selected frames upstream.

The architecture question is whether the device is an independently testable vision
module or an inseparable coprocessor on the main carrier. Prefer the module boundary
until measurements prove integration is worth it.

## Required external circuitry and design inputs

Before schematic entry, extract from the EV-kit design files:

- complete SIMO inductor/capacitor network and all rail decoupling;
- crystals/oscillators and load components;
- reset, boot, SWD, and RISC-V JTAG access;
- flash/programming/recovery path;
- USB high-speed protection and routing;
- camera connector, CSI termination/routing, and 2.5 V rail;
- all required pull-ups, pull-downs, no-connects, and exposed-pad/ground behavior;
- optional SRAM, SD, audio, and power-accumulator blocks worth omitting from a module.

## Handling at home — required controls

### Right now

**Leave the part sealed.** There is no benefit to opening a bare BGA before the
footprint, assembler, and bring-up plan are ready. The sealed protective package is the
best home storage you currently have.

Before opening, photograph and record:

- part label, lot/date code, quantity, and package orientation;
- MSL/peak-temperature label;
- bag seal condition;
- humidity-indicator card result and desiccant presence, if supplied.

### Minimum home ESD work area

Set up one small, controlled bench area:

- grounded static-dissipative work mat;
- wrist strap with its built-in approximately 1 MΩ safety resistor;
- mat and strap tied to the same common-point/equipment-ground connection;
- outlet/ground verified with an outlet tester;
- grounded-tip temperature-controlled soldering iron;
- ESD-safe tweezers and component trays;
- metalized static-shielding bags for transport/storage;
- ordinary plastics, foam, tape, synthetic cloth, and unnecessary paper kept away.

For seated handling, a wrist strap is the primary personnel-grounding method. Never
improvise a direct wire to ground, defeat the strap resistor, or wear the strap while
working on hazardous live voltage. The MAX78002 work itself should remain
de-energized or SELV/low-voltage.

A mat plus strap is a realistic home capability; a full workplace program, conductive
floor, and ionizer are not required merely to inspect and prototype this device. In very
dry conditions or when unavoidable insulators remain near the work, an ionizer becomes
useful, but it does not replace grounding.

### Moisture control

MAX78002GXE+ is listed as **MSL 3 (168 hours)**. That floor-life clock matters for
reflow exposure, not ordinary shelf life while sealed dry. Therefore:

- do not open the dry pack until the assembly window is planned;
- log the opening date/time;
- keep the original humidity-indicator card and desiccant;
- if not assembled within the qualified floor life, return it to a properly sealed
  moisture-barrier bag with fresh desiccant or have the assembler manage bake/dry-pack;
- do not invent a kitchen-oven bake cycle—confirm package/tray temperature capability
  and the assembler's J-STD-033 process.

A silver metalized shielding bag controls ESD; it is not automatically a qualified
moisture-barrier bag. The label, HIC, desiccant, and seal determine whether it is being
managed as dry pack.

### Physical handling

- Open the bag only on the grounded mat while wearing the verified wrist strap.
- Touch the tray/carrier first; handle the package by its edges with ESD-safe tweezers.
- Never touch or wipe the solder balls.
- Keep the device in its original tray/pocket orientation.
- Do not place it on bare metal, copper-clad board, wood, carpet, household foam, or a
  generic plastic parts box.
- Do not probe loose BGA balls or attempt hand-wiring.
- Return it to shielding/dry packaging immediately after inspection.

## Assembly reality

The 0.8 mm-pitch, 144-ball CSBGA is suitable for professional stencil/reflow assembly
and post-assembly inspection. It is not a good first hand-soldered IC. Before committing
it to a PCB, obtain written confirmation from the assembler for:

- this exact package and land pattern;
- minimum trace/space, via drill, and BGA escape strategy;
- solder-mask-defined versus non-solder-mask-defined pad requirements;
- stencil aperture and paste process;
- X-ray inspection;
- component dry-pack/MSL handling and any bake;
- rework capability and cost.

For learning and firmware/model validation, the official MAX78002EVKIT is the lower-risk
path. The bare samples remain valuable for a later module once the reference design is
understood.

## Likely kill mechanisms

- ESD during unprotected handling, especially before board assembly.
- Moisture exposure beyond floor life followed by reflow.
- Reflow or baking outside the qualified package/tray profile.
- Incorrect multi-rail power connections, sequencing, or SIMO passives.
- Overvoltage on the 1.8 V I/O/analog domain.
- Back-powering through an interface while a device rail is off.
- Missing reset/boot defaults or inaccessible debug/recovery.
- Poor BGA land pattern, escape routing, solder paste, or inspection.
- MIPI/USB routing that ignores the EV-kit/reference layout.

## First bring-up experiment

Do not make the first experiment a custom bare-chip board.

1. Reproduce one official MAX78002 example on the EV kit or equivalent known-good
   hardware.
2. Run an ADI CNN example and record model, input, latency, energy, memory, and tool
   versions.
3. Run one PlatypusOne-relevant classifier using a fixed image set.
4. Define the host boundary and measure transport overhead.
5. Compare value against UNO Q-only inference.
6. Only then write a module/carrier requirements sheet and schematic.

**Pass criterion:** the device produces a repeatable, useful inference result with a
measured energy/latency advantage that justifies its integration burden.

## Open questions / schematic blockers

- Dry-pack label, lot/date code, HIC, and storage state.
- EV kit availability versus using the samples only for eventual assembled boards.
- Which PlatypusOne workload is small enough for the MAX78002 but frequent enough to
  justify a dedicated accelerator.
- Camera ownership and host link.
- Toolchain/model-conversion maturity for the selected workload.
- PCB stack-up and assembly cost for a BGA module.
- Full rail current budget and thermal behavior under target inference.
- Any device errata applicable to the received revision.

## Sources

- [Analog Devices MAX78002 product page](https://www.analog.com/en/products/max78002.html)
- [MAX78002 datasheet, Rev. 0](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX78002.pdf)
- [MAX78002 package drawing 21-0163](https://mds.analog.com/api/public/content/csbga-cu_21-0163.pdf)
- [ADI reliability report for MAX78002GXE+](https://www.analog.com/en/reliability/Product/GeneratePdf?datasheetLink=https://www.analog.com/en/MAX78002/datasheet&modelName=MAX78002GXE%2B&modelType=nonautomotive)
- [Analog Devices MAX78002EVKIT resources](https://www.analog.com/en/products/max78002.html#product-evaluationkit)
- [DigiKey MAX78002GXE+ attributes, including MSL 3 / 168 h](https://www.digikey.com/en/products/detail/analog-devices-inc-maxim-integrated/MAX78002GXE/16900190)
- [ESD Association: basic ESD control procedures](https://www.esda.org/esd-overview/esd-fundamentals/part-3-basic-esd-control-procedures-and-materials/)
- [Analog Devices AN-397: ESD prevention and grounded workstation guidance](https://www.analog.com/AN-397)
