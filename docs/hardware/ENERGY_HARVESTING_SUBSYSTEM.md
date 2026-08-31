# PlatypusOne Energy-Harvesting Subsystem

Status: **future architecture / post-contest subsystem**  
Date: **2026-08-30**

## Purpose

Energy harvesting is not a replacement for PlatypusOne's main battery and is not intended to run the high-power compute/display/camera domain directly.

The preferred architecture is a **dual-domain power system**:

1. **Main battery domain** — supplies the high-power PlatypusOne workload: UNO Q, display, camera, illumination, radios, and other burst loads.
2. **Always-on low-power domain** — supplied preferentially by harvested energy when available and backed by local energy storage. This domain remains alive while the main compute system is off or asleep.

Excess harvested energy may slowly charge shared storage/main battery when the always-on domain is satisfied.

## Core architectural principle

> Harvested energy should first make PlatypusOne more persistent, observant, and autonomous — not attempt to replace the battery that handles the big work.

This keeps the physics realistic while making even milliwatt-class sources useful.

## Proposed power topology

```text
                   ENERGY SOURCES

 USB-C PD -----------------------------------+
                                             |
 Thermal / TEG ---- harvester front end -----|
 Solar ---------- harvester front end -------|----> ENERGY MANAGER
 Future source --- harvester front end -------|          |
                                                        +---------------------+
                                                        |                     |
                                                        v                     v
                                              Harvest buffer / supercap   Main battery
                                                        |                     |
                                                        v                     |
                                               ALWAYS-ON MCU DOMAIN           |
                                                        |                     |
                                               wake / monitor / log           |
                                                        |                     |
                                                        +---- wake/control ---+
                                                                              |
                                                                              v
                                                                     MAIN POWER DOMAIN
                                                                  UNO Q / display / camera
```

Exact rail sharing, ORing, charger topology, and storage architecture remain TBD pending bench characterization.

## Always-on MCU role

A dedicated ultra-low-power supervisory MCU is the primary useful load for harvested energy.

Candidate responsibilities:

- monitor battery state and harvested power
- track cumulative harvested energy
- monitor temperature and thermal gradient
- detect motion / orientation changes from a low-power IMU
- monitor trigger, dock, or accessory events
- maintain a low-rate timestamped event log
- advertise minimal BLE/status information if energy budget allows
- decide when to wake the UNO Q / main compute domain
- manage charging or transfer of excess harvested energy to larger storage
- expose energy telemetry to PlatypusOS after wake

The always-on controller should be designed around a **micro-watt to low-milliwatt operating regime**, with aggressive sleep and event-driven wake behavior.

## Main battery role

The main Li-ion/LiPo battery remains responsible for high-energy functions including:

- UNO Q / Linux compute
- main display and backlight
- camera capture and image processing
- illumination LEDs
- high-duty-cycle radios
- radar and other higher-power sensor modules
- sustained UI interaction

Energy harvesting is therefore an **endurance and persistence subsystem**, not the primary propulsion source for the handheld.

## Energy priority concept

A future energy manager should support policy approximately like:

1. Keep the always-on domain alive from harvested energy when possible.
2. Maintain a small local energy reserve for wake/event handling.
3. Route excess harvested energy into shared storage or the main battery when practical.
4. Never destabilize the main system by attempting to operate high-power loads directly from an intermittent harvester.
5. Permit USB-C to remain the authoritative high-power charging source.

## First harvester experiment: thermoelectric

The MATRIX Prometheus family is a strong first experimental path because it combines a thermoelectric generator with an ultra-low-voltage boost architecture intended for small thermal gradients.

Reference datasheet:
https://www.mouser.com/datasheet/3/3671/1/DS_Prometheus.20220218.D.U.pdf

Prometheus should be treated as an **experimental source module**, not a locked production component.

Primary PlatypusOne test cases:

- human-hand / ambient gradient
- warm electronics enclosure
- motor / gearbox housing
- HVAC duct or pipe
- machine frame adjacent to a heat source
- solar-heated outdoor surface

The most compelling engineering use case is likely **industrial waste heat**, where PlatypusOne or a future detachable monitoring node is attached to equipment that simultaneously provides the thermal gradient being measured.

## Mechanical implication

Do not fully encapsulate the final enclosure in thermally insulating polymer.

Preserve at least one mechanically useful thermal interface area in the final industrial design, potentially a metallic chassis spine, rear plate, accessory shoe, or replaceable module interface.

A future thermal path could support both:

- normal product cooling: electronics -> chassis -> ambient
- harvesting mode: hot external surface -> TEG -> exposed chassis/heatsink -> ambient

The harvester must see a usable temperature differential through the device. Simply placing a TEG inside a uniformly warm enclosure is not useful.

## Supercapacitor / burst-energy concept

Harvesting becomes substantially more valuable when energy is accumulated and then spent in discrete useful actions.

Potential low-power-domain actions include:

- wake supervisory MCU
- sample temperature / IMU
- record an event
- transmit a short BLE beacon
- briefly power a sensor
- capture a low-energy measurement
- wake the main compute system only when thresholds justify it

This aligns with the broader Platypus concept of **energy-budgeted modules**: modules need a known number of joules to perform one useful operation rather than requiring continuous high-power supply.

## Engineering Scout connection

The energy-harvesting subsystem should be compatible with future batteryless or near-batteryless Engineering Scout modules.

A Scout module could:

1. accumulate energy from PlatypusOne, a harvester, or both;
2. store a known energy budget in a supercapacitor;
3. detach;
4. perform one or more bounded operations (measure, photograph, transmit, log);
5. return to PlatypusOne or recharge from its environment.

This is a future architecture concept only and must not expand the current contest build scope.

## Energy telemetry interface

PlatypusOS should eventually expose energy harvesting as a first-class measurement source.

Suggested fields:

```text
battery_voltage
battery_soc
battery_power_w
usb_power_w
harvest_source
harvest_voltage
harvest_current
harvest_power_w
harvest_energy_session_j
harvest_energy_total_j
thermal_hot_c
thermal_cold_c
thermal_delta_c
always_on_domain_power_w
```

This opens a useful engineering feature beyond simply charging the device: PlatypusOne can evaluate whether an environment can support autonomous instrumentation.

Example future workflow:

```text
Surface temperature: 57.2 C
Ambient / sink temperature: 24.1 C
Thermal delta: 33.1 C
Harvested power: 18 mW
Estimated daily energy: 1.55 kJ

Assessment:
Environment may support a low-duty-cycle batteryless condition-monitoring node.
```

## Generic harvesting interface

Do not make the architecture Prometheus-specific.

The final carrier PCB should eventually provide a generic harvesting input so that thermoelectric, photovoltaic, vibration, inductive, or other experimental harvesters can share a common energy-management layer.

Future source classes may include:

- thermoelectric
- solar / photovoltaic
- vibration / piezoelectric
- inductive
- extremely-low-power RF harvesting
- mechanical / user-actuated generation

Only sources that demonstrate useful energy density and acceptable packaging complexity should reach the production BOM.

## Rev A guardrails

This subsystem is **not a Rev A contest deliverable**.

For the current design:

- do not add Prometheus to the required BOM;
- do not allow energy harvesting to delay the contest build;
- preserve future PCB/interface flexibility where inexpensive;
- avoid enclosure decisions that unnecessarily eliminate a future thermal interface;
- consider reserving space/test points for a future low-power supervisor and harvesting input only if doing so is low-risk.

## Future validation gates

Before any harvesting subsystem becomes a production requirement, demonstrate:

1. measured output versus thermal gradient for realistic PlatypusOne geometries;
2. cold-start behavior under representative conditions;
3. conversion efficiency into selected storage;
4. harvested-energy contribution over a realistic workday;
5. always-on-domain power budget below sustainable harvested output for at least one meaningful use case;
6. acceptable thermal, EMC, mechanical, and cost impact;
7. no degradation of main battery charging or main-system stability.

## Success criterion

Energy harvesting succeeds if it allows PlatypusOne to remain meaningfully alive, observant, or recover energy while the high-power system is asleep.

It does **not** need to operate the full handheld continuously to justify inclusion.

The long-term objective is an engineering platform that can **collect, store, measure, route, and exploit environmental energy**, with the main battery providing the heavy lifting and the harvesting subsystem extending persistence and enabling ultra-low-power autonomous behavior.
