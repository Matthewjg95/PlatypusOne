# services/ai

Fastener classification and nominal-size matching for Engineering Scout
(`platypus::ai::FastenerClassifier`). It takes a `vision::ScoutAnalysis` and
infers fastener family and likely metric size. Rule-based and deterministic
for v1. Every output is marked as inferred evidence, with confidence and
provenance.

Tests: `tests/test_fastener_classifier.cpp`. Layering rule: depends only on
platform interfaces and other services.
