# PlatypusOne Extension — 24 GHz Doppler Radar

Status: **deferred extension / not Core**  
Date: **2026-08-30**

## Concept

Evaluate a 24 GHz Doppler radar module as a future PlatypusOne sensing extension after the contest Core is reliable.

The current reference platform is the **Infineon DEMO SENSE2GOL PULSE** based on the BGT24LTR11 radar transceiver. It provides an assembled manufacturer development platform with integrated antennas, I/Q acquisition, firmware/GUI support, and motion/speed/direction sensing without requiring a custom RF PCB.

## Why it fits PlatypusOne

Radar could add a non-contact physical observation channel for situations where optical sensing is weak or where motion itself is the engineering signal.

Potential future uses:

- motion / presence characterization;
- speed and direction measurement;
- rotating machinery or mechanism observation;
- non-contact movement verification;
- obscured or low-light target sensing;
- comparative experiments against camera / ToF observations;
- a future detachable or internal PlatypusOne radar module.

Radar should feed the same **Engineering Observation** model used by the Core device rather than becoming a disconnected app.

Potential evidence chain:

`physical target -> radar I/Q evidence -> measured motion/speed/direction -> interpretation + uncertainty -> saved Engineering Observation`

## Reference RoadTest candidate

**Infineon DEMO SENSE2GOL PULSE / BGT24LTR11**

Why it was considered:

- complete assembled RF development board;
- approximately $167 at Newark during the 2026 element14 Open Call search;
- 24 GHz radar front end and antennas already validated by Infineon;
- access to I/Q radar signals and DSP workflow;
- useful reference design for future custom radar hardware.

## Why it is deferred

The Autodesk contest Core already has a coherent sensing stack centered on:

- camera;
- ToF distance/reference;
- IMU orientation;
- controlled illumination;
- Engineering Scout / Engineering Observation workflow.

Adding radar now would increase feature count without closing a current Core requirement. It should only graduate into the contest build if a later test demonstrates that it materially improves the primary demo with negligible schedule and integration risk.

## Graduation criteria

Radar can move from Extension to Core only if all are true:

1. PlatypusOne Core is already reliable end-to-end.
2. The radar solves a demonstrated observation problem that camera + ToF + IMU cannot solve adequately.
3. Integration does not threaten enclosure, power, software, or contest schedule.
4. Radar outputs are incorporated into the Engineering Observation contract with clear provenance and uncertainty.
5. The feature makes the main contest demonstration clearer rather than broader-but-less-coherent.

## Roadmap note

Treat the Sense2GoL Pulse as a **reference/evaluation platform**, not necessarily the production PlatypusOne radar hardware. If the extension proves valuable, later work can evaluate smaller BGT24LTR11-based hardware or a purpose-built radar module after the contest.