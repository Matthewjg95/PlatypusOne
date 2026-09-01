# Carrier Board Development Methodology

Status: **Proposed**
Owner: Matthew Garza
Applies to: PlatypusOne carrier-board experiments and revisions
First tracked component: [MAX78002GXE+](components/MAX78002GXE_PLUS.md)

## Purpose

The carrier board is a learning platform and integration tool before it becomes product
hardware. Rev A prioritizes observability, recoverability, and inexpensive correction
over density. Each later revision must be justified by evidence collected from the
previous one.

- **Rev A — maximum learning:** modular, over-instrumented, easy to isolate and bodge.
- **Rev B — corrected engineering prototype:** closes measured Rev-A defects.
- **Rev C — product-oriented hardware:** size, cost, assembly, and reliability optimization.

This work does not replace the UNO Q contest core or force every received sample onto
the contest board. Components can be catalogued and evaluated without receiving a
Rev-A footprint.

## Source of truth and inventory workflow

The hardware-inventory chat may be used to discover, classify, and draft analyses. The
repository is the controlled engineering record.

For each physical component:

1. Capture the exact manufacturer part number, quantity, package label, date received,
   source, storage state, and clear label/package photos.
2. Search the BOM and component dossiers before creating a duplicate.
3. Create or update one dossier in `docs/hardware/components/`.
4. Separate **verified facts**, **engineering inference**, and **open questions**.
5. Link primary sources: manufacturer datasheet, errata, package drawing, reference
   schematic/layout, evaluation-board design files, and software/toolchain.
6. Assign a PlatypusOne disposition:
   - `CORE`: required for the bounded contest build.
   - `DE-RISK`: test now because it can retire a core design risk.
   - `RESEARCH`: useful future capability; no Rev-A commitment.
   - `HOLD`: insufficient value/evidence or incompatible with the architecture.
7. Update BOM acquisition state only from physical evidence:
   `PLANNED → ORDERED → RECEIVED → TESTED`.
8. Convert every unresolved claim that can affect the schematic into a design question
   or test. Never silently convert an inventory-chat statement into a design decision.

## Development gates

A gate closes only when its exit evidence is committed or linked.

### G0 — Define the job

Document:

- host board and connector boundary;
- supported modules/components;
- input power and required rails;
- interfaces and voltage domains;
- mechanical envelope and mounting;
- Rev-A must-haves, options, and deferred features.

**Exit evidence:** bounded Rev-A scope and first block diagram.

### G1 — Component bring-up dossiers

Every major IC receives a dossier containing:

- exact orderable part and package;
- intended role and architecture disposition;
- supply, reset, clock, boot, and programming requirements;
- I/O voltage levels and communications;
- required passives and reference circuits;
- thermal, layout, assembly, ESD, and moisture controls;
- evaluation hardware/software and known errata;
- likely kill mechanisms;
- bring-up test and unresolved questions.

**Exit evidence:** no major schematic block lacks a reviewed dossier.

### G2 — Block diagram and interface budget

Create the functional block diagram before the schematic. Inventory I2C addresses,
SPI chip-selects, UARTs, USB endpoints, GPIO/interrupt lines, ADC channels, bandwidth,
timing ownership, and every level-shift boundary.

**Exit evidence:** each interconnect has an owner, direction, voltage, expected rate,
default state, and test access strategy.

### G3 — Power architecture

Treat power as a subsystem:

- entry protection and reverse-polarity strategy;
- regulators, rail sequencing, enables, and safe defaults;
- worst-case and measured current budget;
- bulk/local decoupling;
- analog/digital/RF grounding;
- per-rail isolation and current-measurement points;
- shutdown, brownout, charging, and fault behavior.

**Exit evidence:** reviewed power tree, calculated budget, and measurement plan.

### G4 — Schematic by functional sheet

Use separate sheets for power entry, regulation, host interface, each major IC,
sensors, debug, and expansion. Follow manufacturer reference circuits unless a
deviation is explicitly justified.

Rev-A design-for-debug features should include, where appropriate:

- labeled test points and ground points;
- 0-ohm configuration links;
- removable rail/bus jumpers;
- series resistor footprints on fast or uncertain lines;
- programming/debug headers;
- bus exposure and current-measurement opportunities;
- diagnostic LEDs only where they answer a bring-up question.

**Exit evidence:** schematic review checklist closed; no unresolved voltage or default
state.

### G5 — Pre-layout review

Attack the design before placement:

- wrong or missing voltage domain;
- missing pull-up/down or floating enable;
- reset/boot state conflict;
- decoupling or bulk-capacitor omission;
- connector mirroring or pin-one ambiguity;
- inaccessible debug interface;
- unsupported package/footprint or assembly process;
- conflict with enclosure, camera, display, or RF keep-outs.

**Exit evidence:** review log and approved netlist/footprint set.

### G6 — Layout, DFM, and assembly review

Placement order: mechanical constraints and connectors, power, sensitive analog/RF,
major ICs, buses, remaining signals, planes, and test points. Use the manufacturer's
recommended land pattern and reference layout. Obtain assembler confirmation for BGA,
fine-pitch, via, stack-up, stencil, inspection, rework, and moisture-handling limits
before ordering.

**Exit evidence:** DRC clean; schematic/layout cross-check complete; assembler/board
house capability confirmed; fabrication outputs independently reviewed.

### G7 — Bring-up plan before purchase

Write the exact procedure before releasing Gerbers:

1. Packaging, ESD, and visual inspection.
2. Unpowered resistance-to-ground and continuity checks.
3. Current-limited power with all isolatable loads disconnected.
4. Verify rails, sequencing, reset, clocks, and thermal behavior.
5. Connect debugger and prove the smallest firmware image.
6. Enable one peripheral or subsystem at a time.
7. Record expected value, measured value, tolerance, setup, pass/fail, anomaly,
   corrective action, and artifact path.

**Exit evidence:** executable bring-up checklist and acceptance limits.

### G8 — Characterization and revision close

A board that boots is not characterized. Exercise normal, worst-case, and fault
conditions; log every bodge, dead footprint, unexpected measurement, assembly problem,
and unnecessary component.

**Exit evidence:** Rev-A report, disposition for every anomaly, and evidence-backed
Rev-B change list.

## Component dossier template

```markdown
# <Exact manufacturer part number>

Status: RECEIVED / TESTED / ...
Disposition: CORE / DE-RISK / RESEARCH / HOLD
Quantity: <value or TBD>
Storage: <sealed MBB, shielding bag, dry cabinet, etc.>
Evidence date: YYYY-MM-DD

## Why it matters to PlatypusOne
## Verified identity and package
## Functional summary
## Supplies, sequencing, reset, and clocks
## Interfaces and voltage domains
## Required external circuitry
## Layout, thermal, assembly, ESD, and moisture handling
## Software, evaluation path, and reference designs
## Likely kill mechanisms
## Bring-up experiment
## Open questions / schematic blockers
## Sources
```

## Milestone exit criteria

The **Carrier Board Rev-A Methodology** milestone is complete when:

- this method is accepted;
- the physical inventory is reconciled into dossiers and BOM states;
- every Rev-A candidate has a disposition;
- the Rev-A job, block diagram, interface budget, and power tree are committed;
- the schematic and pre-layout reviews close;
- the bring-up procedure exists before fabrication release.

The milestone is documentation and design readiness. It does not require placing every
received sample on the first carrier.
