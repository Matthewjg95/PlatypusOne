# services/ai

Fastener classification and nominal-size matching for Engineering Scout
(`platypus::ai::classify`). It takes a `vision::ScoutAnalysis` and
infers fastener family and likely metric size. Rule-based and deterministic
for v1. `platypus::ai::appendClassification` writes inferred claims with confidence,
provenance and method, and preserves unresolved ambiguities.

Tests: `tests/test_fastener_classifier.cpp`. Layering rule: depends only on
platform interfaces and other services.
