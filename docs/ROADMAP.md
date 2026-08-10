# PlatypusOS Roadmap

Milestone-ordered. Items are tagged `area/topic` for issue tracking.

> **Contest context:** this project targets the Hackster "Build the Autodesk
> University 2027 Product" contest (submission due **Dec 20, 2026**; hardware
> application due **Aug 23, 2026**). See
> [contest snapshot](contest/autodesk-au2027-contest-snapshot.md),
> [hardware BOM](hardware/BOM.md),
> [test checklists](hardware/TEST_CHECKLISTS.md), and the
> [acquisition roadmap](hardware/ACQUISITION_ROADMAP.md).

## M0 — Foundation (this commit)

- [x] Repository structure and layered CMake build
- [x] HAL interfaces: IBoard, IDisplay, ICamera, ISensor/ISensorHub,
      IAudioOutput, IMcuBridge, IStorage, Result/Status
- [x] Renderer skeleton (RGB565 framebuffer, primitives)
- [x] App framework: IApp, AppContext, AppRegistry, manifest
- [x] Launcher + Settings reference app
- [x] HostSimBoard (hardware-free development)
- [x] Assert-based unit tests, architecture docs

## M1 — Board bring-up

- [x] `board/uno-q` — UnoQBoard skeleton (storage + MCU bridge wired; display/
      camera/sensors return nullptr pending their drivers below)
- [x] `board/mcu-bridge` — framing codec + Linux SerialMcuBridge driver;
      protocol spec in docs/protocols/mcu-bridge.md
- [ ] `board/mcu-firmware` — STM32-side counterpart implementing the spec
- [ ] `display/driver` — panel driver (SPI/DSI per final hardware) + touch
- [ ] `camera/libcamera` — libcamera/V4L2 backend for ICamera
- [ ] `sensors/imu` — first real ISensor driver via the MCU bridge
- [ ] `infra/cross-compile` — aarch64 toolchain file + CI build
- [ ] `infra/test-framework` — vendor Catch2 into third_party
- [ ] `infra/format` — commit .clang-format + ruff config, enforce in CI

## M2 — Runtime maturity

- [ ] `renderer/font-atlas` — real bitmap font, text metrics, scaling
- [ ] `renderer/dirty-rects` — partial present for SPI panel bandwidth
- [ ] `appfw/event-queue` — thread-safe event handoff from driver threads
- [ ] `appfw/lifecycle` — pause/resume, low-memory notifications
- [x] `sim/window` — native Win32 window backend for HostSimBoard (mouse =
      touch, number keys = buttons); SDL/X11 backend still open for POSIX hosts
- [ ] `filesystem/settings-store` — typed key/value settings persistence
- [ ] `services/export` — STL/OBJ/PLY writers over geometry::Mesh

## M3 — Flagship apps

- [ ] `apps/shadowscan` — structured-shadow 3D capture pipeline
      (camera → services/vision silhouette extraction → geometry → mesh)
- [ ] `services/vision` — C++ image ops core; Python prototyping sandbox
- [ ] `apps/viewer` — mesh/point-cloud viewer on the renderer
- [ ] `apps/measurement` — sensor-driven measurement with unit management
- [ ] `apps/inspection` — generic live-sensor dashboard (auto-renders any
      SensorDescriptor — the sensor-expansion proof point)
- [ ] `apps/documentation` — photo + note capture into ProjectStore

## M4 — Platform opening

- [ ] `appfw/plugins` — dlopen app loading behind AppFactory ABI; signed
      manifests; capability permissions per app
- [ ] `services/ai` — on-device inference service (defect detection assist)
- [ ] `platform/power` — battery gauge, sleep states, IPowerManager
- [ ] `infra/ota` — A/B update strategy for the Linux image
- [ ] SDK: out-of-tree app template + docs for third-party developers
