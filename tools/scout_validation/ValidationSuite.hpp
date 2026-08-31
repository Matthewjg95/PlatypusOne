// Scout validation suite — MVP build-order step 8.
//
// A repeatable battery of synthetic fastener scenes with exact ground truth,
// run through the real analyzer + classifier. Reports dimensional error and
// classification hit rate, and includes intentional failure cases where the
// CORRECT behavior is a typed refusal — a failure case "passes" when the
// pipeline refuses for the expected reason (DigiKey winning target: at least
// one intentional failure demonstrated).
//
// Every case carries an explicit expectation, including expected misses for
// documented v1 limitations (e.g. a 45°-rotated hexagon's extent-based
// across-flats overestimate). The suite is deterministic: same binary, same
// results, so CI can gate on it.
#pragma once

#include <platypus/ai/FastenerClassifier.hpp>
#include <platypus/vision/ScoutAnalyzer.hpp>

#include <optional>
#include <string>
#include <vector>

namespace platypus::validation {

struct CaseResult {
    std::string name;
    std::string kind;  ///< "measurement" or "failure"

    // Expectations.
    std::optional<vision::AnalyzeError> expectedError;  ///< failure cases
    std::optional<ai::FastenerClass> expectedClass;
    std::optional<std::string> expectedNominal;  ///< nullopt = no nominal expected
    double truthLengthMm = 0.0;                  ///< 0 = not applicable
    double truthWidthMm = 0.0;

    // Actuals.
    std::optional<vision::AnalyzeError> actualError;
    std::optional<ai::FastenerClass> actualClass;
    std::optional<std::string> actualNominal;
    double measuredLengthMm = 0.0;
    double measuredWidthMm = 0.0;

    // Verdicts.
    bool behaved = false;  ///< everything matched the expectation
    std::string note;      ///< limitation/context shown in the report
};

struct Summary {
    std::size_t cases = 0;
    std::size_t behaved = 0;
    std::size_t classApplicable = 0;
    std::size_t classHits = 0;
    std::size_t nominalApplicable = 0;
    std::size_t nominalHits = 0;
    std::size_t failureCases = 0;
    std::size_t failureBehaved = 0;
    double meanAbsLengthErrMm = 0.0;
    double maxAbsLengthErrMm = 0.0;
    double meanAbsWidthErrMm = 0.0;
    double maxAbsWidthErrMm = 0.0;
};

struct ValidationRun {
    std::vector<CaseResult> results;
    Summary summary;
};

/// Runs the full battery. Deterministic and hardware-free.
[[nodiscard]] ValidationRun runValidation();

/// Markdown report of one run (no timestamps — regenerating on the same code
/// must produce a byte-identical document).
[[nodiscard]] std::string formatReport(const ValidationRun& run);

}  // namespace platypus::validation
