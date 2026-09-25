# services/vision

Scout analyzer (`platypus::vision::ScoutAnalyzer`): calibrates mm-per-pixel
scale from a known reference square and measures the subject's principal-axis
extents. Deterministic geometry only (Otsu, connected components). No ML and
no external dependencies. The v1 scene contract is documented in
`ScoutAnalyzer.hpp`.

Tests: `tests/test_scout_analyzer.cpp`, `tests/test_scout_validation.cpp`.
Layering rule: depends only on platform interfaces and other services.
