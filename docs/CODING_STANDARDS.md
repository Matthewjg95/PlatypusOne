# PlatypusOS Coding Standards

## C++

- **Standard:** C++20, no compiler extensions. Warnings-as-clean under
  `-Wall -Wextra -Wpedantic -Wshadow -Wconversion` (see
  [CompilerWarnings.cmake](../cmake/CompilerWarnings.cmake)).
- **Namespaces:** everything under `platypus::`, one sub-namespace per layer
  (`hal`, `renderer`, `appfw`, `apps`, …).
- **Headers:** `#pragma once`; include what you use; public headers live in
  `include/platypus/<module>/`.
- **Ownership:** `std::unique_ptr` for exclusive, `std::shared_ptr` only for
  genuinely shared hardware handles handed out by `IBoard`. Raw pointers never
  own.
- **Errors:** `Result<T>`/`Status` across module boundaries; exceptions never
  cross a HAL or plugin boundary.
- **No globals, no singletons.** Dependencies are injected. The lone exception
  is the `sig_atomic`-style shutdown flag in the composition root.
- **Interfaces:** pure-virtual, virtual destructor, no data members, prefixed
  `I`. Callbacks documented with their executing thread.
- **Naming:** `PascalCase` types, `camelCase` functions/methods,
  `snake_case_` no — members use `camelCase_` trailing underscore,
  `kPascalCase` constants.
- **Formatting:** clang-format 18 (Google-derived, 4-space indent, 100 cols) —
  repository policy is defined in [`.clang-format`](../.clang-format).

## Python (services/ai, services/vision prototyping only)

- Python 3.11+, `ruff` + `mypy --strict`, type hints mandatory. Ruff lint and
  formatting policy is defined in [`pyproject.toml`](../pyproject.toml).
- Modules expose a narrow functional API consumed via the C++ boundary; no
  Python code imports hardware directly.

## Documentation

- Every public header opens with a purpose comment: what the module is, who
  calls it, and which thread callbacks run on.
- Every module directory gets a `README.md` once it contains real logic.
- Architecture-level decisions get an ADR in [docs/adr/](adr/README.md) —
  process and template live there.

## Testing

- Every service must be testable with fakes of the HAL interfaces alone —
  see [test_renderer.cpp](../tests/test_renderer.cpp) for the pattern.
- Tests run on the host; no test may require hardware.
