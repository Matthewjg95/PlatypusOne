# PlatypusOne Hardware BOM Scope Layers

Status: **scope control**
Date: **2026-09-01**

PlatypusOne currently has three related hardware views. They share evidence, but they are not interchangeable BOMs.

| Scope | Purpose | Authoritative record | Received ADI samples |
|---|---|---|---|
| **1. Hardware Request BOM** | The beginning-of-BOM shown in the hardware application to demonstrate feasibility and request UNO Q, Fusion, and PCBWay support | [Hardware Application Checklist §4](../contest/HARDWARE_APPLICATION_CHECKLIST.md#4-bom-excerpt-for-the-project-page-agent) | **Excluded.** Receiving a sample does not change what the application asks for. |
| **2. Contest Core Build** | The bounded, manufacturable December submission that proves the observation-to-artifact loop | [PlatypusOne Core Scope](../contest/PLATYPUSONE_CORE_SCOPE.md) | **Excluded by default.** A component enters only through the recorded Core scope-change test. |
| **3. Post-contest Platform / Research Build** | The broader PlatypusOne product, carrier experiments, daughterboards, industrial interfaces, and future modules | [Master planning BOM](BOM.md), component dossiers, and carrier methodology | **Catalogued here.** Research and de-risk work may proceed without creating a Core requirement. |

## Scope rules

1. The hardware-request table is curated application copy, not a purchase order and not a mirror of every owned component.
2. Contest Core is role-based first. Specific parts graduate only after validation and schedule review.
3. The master planning BOM is a **superset**: candidates, alternates, received inventory, tools, stretch features, and post-contest parts may coexist there.
4. RECEIVED records possession. It does not mean CORE, APPROVED, SELECTED, or READY FOR SCHEMATIC.
5. No dossier, inventory-chat entry, or BOM row silently changes another scope.

## Promotion path

A received component moves through the following evidence gates:

**INVENTORY → DOSSIER → BENCH TEST → ARCHITECTURE DECISION → SELECTED BUILD**

Promotion into Contest Core additionally requires the five-question test in
[PlatypusOne Core Scope §11](../contest/PLATYPUSONE_CORE_SCOPE.md#11-scope-change-test)
and a recorded ADR or explicit scope-document change.

## Current received-component mapping

| Component | Qty | Request BOM | Contest Core | Post-contest / research disposition |
|---|---:|---|---|---|
| MAX78002GXE+ | 2 | No | No | V2 low-power vision/inference research |
| ADXL357BEZ | 2 | No | No by default | Precision vibration/tilt daughterboard de-risk |
| LTC2949ILXE#3ZZPBF | 2 | No | No | Switched power-characterization pod / future high-power monitoring |
| ADM2587EBRWZ | 2 | No | No | Isolated industrial RS-485 daughterboard de-risk |

## Change control

When a document says “the BOM,” it must identify one of the three scopes above.
Application copy must cite the Hardware Request BOM. December design and ordering
must cite Contest Core. Research intake and long-range planning must cite the
master planning BOM and the relevant dossier.
