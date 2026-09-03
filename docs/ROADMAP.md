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
- [ ] `display/linked` — **host side done** (2026-09-02): `LinkedDisplay` +
      POSIX serial transport, loopback-tested, injectable into `UnoQBoard`
      via `--board unoq --display-link PATH`. Remaining: Tab5 client
      firmware + physical session (status board step 5)
- [ ] `display/driver` — integrated panel driver + touch. **Deferred** until a
      production panel is selected; do not start before ADR-0001 is superseded
- [ ] `camera/libcamera` — libcamera/V4L2 backend for ICamera
- [ ] `sensors/imu` — first real ISensor driver via the MCU bridge
- [ ] `infra/cross-compile` — aarch64 toolchain file + CI build
- [ ] `infra/test-framework` — vendor Catch2 into third_party
- [ ] `infra/format` — commit .clang-format + ruff config, enforce in CI

## M1H — Carrier Board Rev-A Methodology

Method, exit criteria, and per-component dossiers live in the private planning
overlay until their components are promoted into Contest Core.

**2026-09-04 milestone:** the related Project Platypus Patch Antenna was approved
for the Tindie × NextPCB Hardware Creator R&D Support Program. Three monthly
rounds provide standalone PCB-fabrication and international-shipping support.
The September round is reserved for Patch Antenna Rev 2. October and/or November
may be redirected to Platypus One carrier/interface fabrication if antenna
validation closes without another evidence-justified spin. The program PCB
voucher does not fund assembly or component procurement, so the Platypus One
carrier plan must continue to control package difficulty and BOM cost separately.

**Learning strategy:** prioritize general CCA development capability, using
KiCad as the implementation tool. The target competency is requirements →
interface table → datasheet/reference circuit → schematic → placement/routing →
DFM/DFA → bring-up → measured revision, rather than KiCad proficiency as an end
in itself.

- [x] Define the evidence-gated Rev A → Rev B → Rev C development method
- [x] Establish per-component dossier and inventory-to-repository workflow
- [x] Create initial received-component dossiers (private planning overlay)
- [x] Establish CCA-first learning strategy with KiCad as the working tool
- [x] Identify potential October/November NextPCB fabrication support path
- [ ] Reconcile the physical hardware inventory into dossiers and BOM acquisition states
- [ ] Assign every candidate CORE / DE-RISK / RESEARCH / HOLD
- [ ] Build one minimal sacrificial KiCad board if needed to close the full fabrication workflow
- [ ] Freeze bounded Rev-A job and functional block diagram
- [ ] Create the Rev-A interface-control table: source, destination, voltage, protocol, current, protection, test access
- [ ] Close interface budget and power tree
- [ ] Reproduce and annotate each CORE device's manufacturer-recommended circuit before custom optimization
- [ ] Complete schematic, ERC, pre-layout, DFM, DFA, and assembly reviews
- [ ] Add accessible power/debug/test points and define current-limited first-power procedure
- [ ] Commit bring-up procedure and acceptance limits before fabrication release
- [ ] Target fabrication-ready Sensor/Interface Carrier Rev A before the October 5 support round
- [ ] Characterize Rev A and derive the Rev-B change list from measured evidence

Rev A should be deliberately conservative: favor proven interfaces, manageable
packages, accessible test points, and modular sensor connections over dense or
novel integration. Advanced power, RF, radar, BGA, and other difficult assembly
features belong in later boards unless they are required by the bounded Core
mission.

This milestone does not place every received sample on Rev A: research
candidates stay in the private planning overlay until an evaluation-platform
experiment and assembly review justify integration.

## M2 — Runtime maturity

- [x] `renderer/font-atlas` — built-in 5×7 bitmap font (printable ASCII,
      hollow-box replacement glyph), integer scaling, textWidth/textHeight
      metrics; visually verified via `ui_preview --font-specimen`
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
