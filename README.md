# PlatypusOS

An extensible embedded operating environment for the **Arduino UNO Q**, built by
Platypus One. PlatypusOS is not a single application — it is an engineering
handheld platform where **every feature is an app**.

## Architecture at a glance

```
┌─────────────────────────────────────────────────────┐
│  apps/        launcher · shadowscan · viewer ·      │
│               measurement · inspection · docs ·     │
│               settings          (all implement IApp)│
├─────────────────────────────────────────────────────┤
│  services/    appfw · renderer · vision · ai ·      │
│               geometry · export · filesystem        │
├─────────────────────────────────────────────────────┤
│  platform/    HAL interfaces + board drivers        │
│               (IBoard · ICamera · ISensorHub ·      │
│                IDisplay · IAudioOutput · IMcuBridge)│
├─────────────────────────────────────────────────────┤
│  Arduino UNO Q:  Linux MPU (QRB2210)  ⇄  STM32U585 │
└─────────────────────────────────────────────────────┘
```

Dependency direction is strict: **apps → services → platform**. Nothing points
upward. Concrete hardware types exist only in board implementations and the
composition root ([main.cpp](apps/launcher/src/main.cpp)).

## Key principles

- **Everything is an app.** Features implement `IApp` and register with
  `AppRegistry`. The launcher is itself just an app.
- **Interfaces over hardware.** No module touches a driver; capabilities come
  through `IBoard` accessors that return `nullptr` when absent.
- **Dependency injection, no global state.** All services flow through
  `AppContext`, built once in the composition root.
- **Host-first development.** `HostSimBoard` lets the whole OS build and run on
  a workstation with zero hardware.
- **Plugin-ready.** `AppFactory` is the future dynamic-loading ABI.

## Building

```
cmake -B build -DPLATYPUS_TARGET_HOST=ON
cmake --build build
ctest --test-dir build
./build/apps/launcher/platypus_launcher
./build/apps/launcher/platypus_launcher --geometry 800x480
```

Requires CMake ≥ 3.22 and a C++20 compiler (GCC 12+, Clang 15+, MSVC 19.3+).
Every subsystem is an independent CMake target and compiles on its own.

`--geometry WxH` simulates a different panel (default 320×240). No resolution
is compiled in: the display target is deliberately dynamic while the production
panel is undecided, and prototyping runs against a linked external display —
see [ADR-0001](docs/adr/0001-dynamic-linked-prototype-display.md) and the
[presentation link protocol](docs/protocols/presentation.md).

## Documentation

- [docs/PRODUCT_REQUIREMENTS_BASELINE.md](docs/PRODUCT_REQUIREMENTS_BASELINE.md) — what Platypus One must be and how each requirement is verified
- [STATUS.md](STATUS.md) — where the project actually is right now
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — layers, diagrams, rationale
- [docs/CODING_STANDARDS.md](docs/CODING_STANDARDS.md) — C++/Python conventions
- [docs/ROADMAP.md](docs/ROADMAP.md) — prioritized TODO roadmap
- Carrier-board methodology, BOM scope layering, and post-contest research
  dossiers live in the private planning overlay until their components are
  promoted into Contest Core
- [docs/adr/](docs/adr/README.md) — architecture decision records
- [docs/protocols/](docs/protocols/) — wire protocols (MCU bridge, presentation link)
- Per-module headers carry interface documentation inline.

## Repository layout

| Path | Contents |
|---|---|
| `platform/` | HAL interfaces and board drivers (hardware, display, camera, sensors, audio) |
| `services/` | Hardware-agnostic engines (appfw, renderer, vision, ai, geometry, export, filesystem) |
| `apps/` | User-facing applications |
| `third_party/` | Vendored dependencies (currently none — kept dependency-free) |
| `docs/` | Architecture, standards, diagrams, roadmap |
| `tests/` | Unit tests (run on host, no hardware needed) |

## License

PlatypusOS is licensed under the [Apache License 2.0](LICENSE). This permits
commercial use, modification, and distribution subject to the license terms.
The license does not grant rights to project names or trademarks.
