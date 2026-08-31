// Validation-battery gate: every case in the suite must behave exactly as
// specified — measurement cases hit their expected class/nominal, intentional
// failure cases refuse for the expected reason — and dimensional error stays
// inside the gates. A regression anywhere in the pipeline fails here first.
#include "ValidationSuite.hpp"

#include <cassert>
#include <cstdio>

void test_scout_validation() {
    const auto run = platypus::validation::runValidation();
    const auto& s = run.summary;

    // Print any misbehaving case before asserting so failures are actionable.
    for (const auto& result : run.results)
        if (!result.behaved) std::printf("  MISBEHAVED: %s\n", result.name.c_str());

    assert(s.cases >= 20);  // "roughly 20 varied objects/captures"
    assert(s.behaved == s.cases);
    assert(s.failureCases >= 4 && s.failureBehaved == s.failureCases);

    // Dimensional gates: raster error at the battery's scales.
    assert(s.meanAbsLengthErrMm <= 0.6);
    assert(s.maxAbsLengthErrMm <= 1.2);
    assert(s.meanAbsWidthErrMm <= 0.6);
    assert(s.maxAbsWidthErrMm <= 1.2);

    // The report must be deterministic.
    assert(platypus::validation::formatReport(run) ==
           platypus::validation::formatReport(platypus::validation::runValidation()));

    std::puts("test_scout_validation: OK");
}
