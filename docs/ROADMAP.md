# PlatypusOS Roadmap

Milestone-ordered. Items are tagged `area/topic` for issue tracking.

> **Contest context:** this project targets the Hackster "Build the Autodesk
> University 2027 Product" contest (submission due **Dec 20, 2026**; hardware
> application due **Sep 7, 2026**, extended from Aug 23). See
> [contest snapshot](contest/autodesk-au2027-contest-snapshot.md),
> [application checklist](contest/HARDWARE_APPLICATION_CHECKLIST.md),
> [hardware BOM](hardware/BOM.md),
> [test checklists](hardware/TEST_CHECKLISTS.md), and the
> [acquisition roadmap](hardware/ACQUISITION_ROADMAP.md).
>
> **Display:** deliberately dynamic — prototyping runs against a linked
> external display until the production panel is chosen
> ([ADR-0001](adr/0001-dynamic-linked-prototype-display.md),
> [presentation link](protocols/presentation.md)).

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
- [ ] `display/linked` — `LinkedDisplay` over the
      [presentation link](protocols/presentation.md) + client firmware; the
      prototype display path per ADR-0001. Its `appfw/event-queue` and
      `renderer/dirty-rects` prerequisites are complete
- [ ] `display/driver` — integrated panel driver + touch. **Deferred** until a
      production panel is selected; do not start before ADR-0001 is superseded
- [ ] `camera/libcamera` — libcamera/V4L2 backend for ICamera
- [ ] `sensors/imu` — first real ISensor driver via the MCU bridge
- [ ] `infra/cross-compile` — aarch64 toolchain file + CI build
- [ ] `infra/test-framework` — vendor Catch2 into third_party
- [ ] `infra/format` — commit .clang-format + ruff config, enforce in CI

## M1H — Carrier Board Rev-A Methodology

Method and exit criteria:
[hardware/CARRIER_BOARD_DEVELOPMENT_METHOD.md](hardware/CARRIER_BOARD_DEVELOPMENT_METHOD.md).

- [x] Define the evidence-gated Rev A → Rev B → Rev C development method
- [x] Establish per-component dossier and inventory-to-repository workflow
- [x] Create first received-component dossiers:
      [MAX78002GXE+](hardware/components/MAX78002GXE_PLUS.md) and
      [ADXL357BEZ](hardware/components/ADXL357BEZ.md)
- [ ] Reconcile the physical hardware inventory into dossiers and BOM acquisition states
- [ ] Assign every candidate CORE / DE-RISK / RESEARCH / HOLD
- [ ] Freeze bounded Rev-A job and functional block diagram
- [ ] Close interface budget and power tree
- [ ] Complete schematic, pre-layout, DFM, and assembly reviews
- [ ] Commit bring-up procedure and acceptance limits before fabrication release
- [ ] Characterize Rev A and derive the Rev-B change list from measured evidence

This milestone does not place every received sample on Rev A. In particular, the bare
MAX78002GXE+ remains a V2 research candidate until an evaluation-platform experiment
and professional BGA assembly review justify integration.

## M2 — Runtime maturity

- [ ] `renderer/font-atlas` — real bitmap font, text metrics, scaling
- [x] `renderer/dirty-rects` — bounded dirty-region tracking with no-op clean
      frames, retry after transport failure, and a full-frame fallback for
      drivers without partial-update support. **Prerequisite, not an
      optimization:** a full 800×480 frame is 768 kB and a USB CDC link carries
      roughly one per second (bandwidth table in
      [presentation.md](protocols/presentation.md))
- [x] `appfw/event-queue` — bounded, thread-safe touch/button handoff from
      driver threads to UI-thread app callbacks
- [ ] `appfw/lifecycle` — pause/resume, low-memory notifications
- [x] `sim/window` — native Win32 window backend for HostSimBoard (mouse =
      touch, number keys = buttons); SDL/X11 backend still open for POSIX hosts
- [x] `sim/geometry` — simulated panel size chosen at runtime
      (`--geometry WxH`), no resolution compiled in (ADR-0001)
- [ ] `appfw/encoder-input` — HAL representation for the rotary encoder, the
      primary navigation control. The presentation link already reserves a
      message for it; `hal::IDisplay` has no event type yet
- [x] `filesystem/settings-store` — typed key/value persistence: line-oriented
      `name=type:value` file, atomic save (temp + rename), typed getters with
      defaults, malformed lines degrade key-by-key
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
