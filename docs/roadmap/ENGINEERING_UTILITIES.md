# Platypus One — Engineering Utilities Roadmap

> Mission: **An engineer standing beside you.**

Platypus One should not expose sensors merely because they exist. Sensors and connectivity are shared platform capabilities that applications combine to answer useful engineering questions.

## V1 — Contest Core

### Perception and geometry
- RGB camera capture and computer vision
- Depth-assisted measurement / geometry capture
- IMU-assisted scan orientation and motion awareness
- Controlled illumination
- ShadowScan workflow
- Interactive 3D model viewer
- Measurement utilities
- Image and engineering-note capture
- Lightweight local inference for object/feature recognition and contextual tool routing

### Connectivity infrastructure
Wi-Fi and Bluetooth are platform infrastructure rather than headline features.
- Project/file transfer to a workstation
- Optional local web UI/dashboard
- Software/model/data updates
- Workstation/Fusion companion communication
- Bluetooth peripheral support where useful
- Offline/local-network operation

## V1 Stretch — Wireless Utility

A small diagnostic utility may expose engineering-useful wireless information without turning Platypus One into an RF-focused product:
- Connected SSID
- RSSI / link quality
- Wi-Fi band/channel where supported
- Connectivity diagnostics
- Local access-point mode for field/offline workflows

The wireless utility should reuse the platform networking service and remain secondary to the core physical-engineering workflow.

## V2 — Connected Engineering Peripherals

- BLE sensor and measurement-tool integration
- Context-aware import from supported peripherals
- Direct transfer of measurements into project records
- Additional local collaboration/device discovery workflows
- Thermal imaging companion module
- Macro inspection module
- Structured-light/depth experimentation

## V3 — Advanced Communications / RF Module

Treat advanced RF as a dedicated companion module with its own radio, RF layout, antenna, and regulatory considerations rather than depending on the UNO Q radio.

Potential utilities:
- RF/site survey workflows
- External-antenna experimentation
- Specialized wireless protocol modules
- LoRa / telemetry tools
- Future spectrum-analysis or SDR-adjacent experiments where suitable hardware is added

This is intentionally a stretch-of-a-stretch and should not distract from the Autodesk contest prototype.

## Platform rule

**The user selects a job, not a sensor.**

Examples:
- Reverse Engineer → camera + depth + IMU + geometry services
- Inspect → camera + macro/thermal module + vision services
- Machine Health → radar + audio + thermal where available
- Document → camera + microphone + OCR/local inference
- Wireless Diagnostics → networking service

## UNO Q antenna note

The current product architecture assumes the UNO Q's supported onboard Wi-Fi/Bluetooth antenna arrangement. Do not make an external antenna on the UNO Q a Rev A requirement. Preserve sensible RF clearance in enclosure packaging, and reserve advanced RF functionality for a future dedicated communications module.
