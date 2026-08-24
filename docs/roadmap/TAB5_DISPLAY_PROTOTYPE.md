# Tab5 as the Platypus One Prototype Display

## Decision

Use the existing M5Stack Tab5 as the first prototype display/control surface while the final Platypus One LCD and enclosure stack remain under evaluation.

This is a development fixture, not the intended production architecture.

## Why

The objective is to get real Platypus One software running against physical hardware immediately. Using a known display platform lets development of workflows, UI, rendering, communication, and interaction begin without prematurely locking the final LCD.

## Prototype architecture

```text
Arduino UNO Q
  Linux / AI / vision / project services
        |
        | prototype transport
        v
M5Stack Tab5
  display / touch / prototype UI / 3D visualization

UNO Q MCU / attached peripherals
  sensor I/O / buttons / future rotary encoder / illumination
```

## Architecture constraint

The UI must not become Tab5-specific.

Create a transport-independent interface between the UNO Q application/services and presentation layer. The Tab5 is one temporary client. A later integrated LCD must be able to replace it without rewriting ShadowScan, inference, geometry, project storage, or engineering utilities.

## First protocol

Start with the lowest-friction reliable transport available during bring-up (USB serial is preferred for the first proof if practical). Define messages rather than sharing application internals.

Suggested early messages:
- heartbeat / device info
- app state
- menu items / selection
- image or thumbnail metadata
- model/file metadata
- measurement results
- inference results
- sensor status
- user input events

Document and version the protocol from the first working build.

## One-week development sprint

### Day 1 — UNO Q bring-up
- Update/verify board software and development environment
- Hello-world application on Linux side
- Verify MCU interaction path
- Record actual interfaces available on the physical board

### Day 2 — UNO Q ↔ Tab5 link
- Establish prototype transport
- Heartbeat/device-info exchange
- Tab5 status screen showing live UNO Q connection

### Day 3 — Platypus shell
- App launcher / navigation state
- Define app/service boundary
- Implement a simple hardware abstraction layer
- Keep display code outside engineering application logic

### Day 4 — 3D Viewer
- Reuse proven Tab5 renderer concepts
- Load a known model
- Rotate / zoom / basic model metadata
- Receive model selection/state from UNO Q

### Day 5 — Camera + perception proof
- Get a supported camera feeding the UNO Q if hardware is available
- Capture a frame
- Run one practical local CV/inference operation
- Return result to Tab5 UI

### Day 6 — ShadowScan integration spike
- Establish the smallest useful ShadowScan workflow
- Capture/import geometry-related input
- Produce an artifact or measurement
- Preview result on Tab5

### Day 7 — Integration demo
Target demo:

`UNO Q boots → Tab5 connects → select engineering app → capture/analyze something → result appears → open/inspect related 3D geometry.`

Record failures and hardware constraints rather than hiding them; these directly inform Rev A BOM and Fusion packaging.

## Success criteria

By the end of the sprint we should know:
1. Whether the UNO Q is a practical compute core for the intended local perception workload.
2. What data rates/interfaces the final display architecture actually needs.
3. Which parts of existing Tab5 rendering/software can be reused.
4. Which peripherals deserve to be locked into the Rev A BOM.
5. Which hardware assumptions need to change before enclosure packaging is frozen.
