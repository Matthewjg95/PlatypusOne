#include "ValidationSuite.hpp"

#include <platypus/hal/testing/SyntheticScene.hpp>

#include <cmath>
#include <cstdio>
#include <functional>

namespace platypus::validation {

namespace {

using ai::FastenerClass;
using hal::testing::SyntheticScene;
using vision::AnalyzeError;
using vision::CalibrationSpec;

constexpr double kPi = 3.14159265358979;

double deg(double degrees) {
    return degrees * kPi / 180.0;
}

struct CaseSpec {
    std::string name;
    std::string kind;  // "measurement" | "failure"
    double mmPerPx = 0.25;
    std::function<void(SyntheticScene&, double /*pxPerMm*/)> build;
    std::optional<AnalyzeError> expectedError;
    std::optional<FastenerClass> expectedClass;
    std::optional<std::string> expectedNominal;
    double truthLengthMm = 0.0;
    double truthWidthMm = 0.0;
    std::string note;
};

/// Reference square sized so the scale comes out at exactly spec.mmPerPx for
/// the 20 mm calibration reference.
void addReference(SyntheticScene& scene, double pxPerMm) {
    const auto side = static_cast<std::int32_t>(std::lround(20.0 * pxPerMm));
    scene.addSquare(24, 24, side);
}

std::vector<CaseSpec> buildCases() {
    std::vector<CaseSpec> cases;

    struct Bolt {
        const char* nominal;
        double diameterMm;
        double lengthMm;
        double angleDeg;
        double mmPerPx;
    };
    // Shaft diameters per ISO 262; lengths and poses varied deliberately.
    const Bolt bolts[] = {
        {"M3", 3.0, 16.0, 10.0, 0.125}, {"M4", 4.0, 20.0, 35.0, 0.2},
        {"M5", 5.0, 25.0, 60.0, 0.25},  {"M6", 6.0, 30.0, 0.0, 0.25},
        {"M8", 8.0, 40.0, 45.0, 0.25},  {"M10", 10.0, 50.0, 20.0, 0.5},
        {"M12", 12.0, 60.0, 75.0, 0.5},
    };
    for (const auto& bolt : bolts) {
        char name[64];
        std::snprintf(name, sizeof(name), "bolt %s x %.0f mm @ %.0f deg", bolt.nominal,
                      bolt.lengthMm, bolt.angleDeg);
        cases.push_back({name, "measurement", bolt.mmPerPx,
                         [bolt](SyntheticScene& scene, double pxPerMm) {
                             addReference(scene, pxPerMm);
                             scene.addRect(400.0, 300.0, bolt.lengthMm * pxPerMm,
                                           bolt.diameterMm * pxPerMm, deg(bolt.angleDeg));
                         },
                         std::nullopt, FastenerClass::BoltOrScrew, bolt.nominal, bolt.lengthMm,
                         bolt.diameterMm, ""});
    }

    struct Nut {
        const char* nominal;
        double acrossFlatsMm;
        double boreMm;
        double angleDeg;
        double mmPerPx;
        bool nominalExpected;
        const char* note;
    };
    // Across-flats per ISO 4032. Rotation is deliberately varied: the
    // minimum-support-width measurement must recover the across-flats
    // regardless of how the nut landed.
    const Nut nuts[] = {
        {"M4", 7.0, 3.3, 0.0, 0.125, true, ""},  {"M5", 8.0, 4.2, 15.0, 0.2, true, ""},
        {"M6", 10.0, 5.0, 0.0, 0.25, true, ""},  {"M8", 13.0, 6.8, 30.0, 0.25, true, ""},
        {"M10", 16.0, 8.5, 10.0, 0.5, true, ""}, {"M12", 18.0, 10.2, 45.0, 0.5, true, ""},
    };
    for (const auto& nut : nuts) {
        char name[64];
        std::snprintf(name, sizeof(name), "nut %s (AF %.0f mm) @ %.0f deg", nut.nominal,
                      nut.acrossFlatsMm, nut.angleDeg);
        cases.push_back(
            {name, "measurement", nut.mmPerPx,
             [nut](SyntheticScene& scene, double pxPerMm) {
                 addReference(scene, pxPerMm);
                 scene.addHexagon(400.0, 300.0, nut.acrossFlatsMm * pxPerMm, deg(nut.angleDeg));
                 scene.addBore(400.0, 300.0, nut.boreMm * pxPerMm / 2.0);
             },
             std::nullopt, FastenerClass::NutOrWasher, std::optional<std::string>(nut.nominal), 0.0,
             nut.acrossFlatsMm, nut.note});
    }

    // Washers: class is nut_or_washer, and the classifier claims an AF-basis
    // nominal that is only valid under the nut interpretation (nut_vs_washer
    // stays unresolved in the record). The battery pins that documented
    // behavior: an M6 washer's 12 mm OD reads as "M8" through the AF table.
    cases.push_back({"washer M6 (OD 12 mm)", "measurement", 0.25,
                     [](SyntheticScene& scene, double pxPerMm) {
                         addReference(scene, pxPerMm);
                         scene.addDisc(400.0, 300.0, 6.0 * pxPerMm);
                         scene.addBore(400.0, 300.0, 3.2 * pxPerMm);
                     },
                     std::nullopt, FastenerClass::NutOrWasher, "M8", 0.0, 0.0,
                     "AF-basis nominal is conditional on the nut interpretation"});
    cases.push_back({"washer M10 (OD 20 mm)", "measurement", 0.5,
                     [](SyntheticScene& scene, double pxPerMm) {
                         addReference(scene, pxPerMm);
                         scene.addDisc(400.0, 300.0, 10.0 * pxPerMm);
                         scene.addBore(400.0, 300.0, 5.3 * pxPerMm);
                     },
                     std::nullopt, FastenerClass::NutOrWasher, "M12", 0.0, 0.0,
                     "AF-basis nominal is conditional on the nut interpretation"});

    // Intentional failure cases: the pipeline must refuse, with the reason.
    cases.push_back({"no reference target", "failure", 0.25,
                     [](SyntheticScene& scene, double pxPerMm) {
                         scene.addRect(400.0, 300.0, 30.0 * pxPerMm, 6.0 * pxPerMm, deg(20.0));
                     },
                     AnalyzeError::NoReferenceTarget, std::nullopt, std::nullopt, 0.0, 0.0,
                     "operator forgot the calibration square"});
    cases.push_back({"two comparable squares", "failure", 0.25,
                     [](SyntheticScene& scene, double pxPerMm) {
                         addReference(scene, pxPerMm);
                         scene.addSquare(400, 60, static_cast<std::int32_t>(18.0 * pxPerMm));
                     },
                     AnalyzeError::ReferenceAmbiguous, std::nullopt, std::nullopt, 0.0, 0.0,
                     "a second square-ish object confuses calibration"});
    cases.push_back({"reference only", "failure", 0.25,
                     [](SyntheticScene& scene, double pxPerMm) { addReference(scene, pxPerMm); },
                     AnalyzeError::NoSubject, std::nullopt, std::nullopt, 0.0, 0.0,
                     "nothing to measure"});
    cases.push_back({"subject touches the reference", "failure", 0.25,
                     [](SyntheticScene& scene, double pxPerMm) {
                         addReference(scene, pxPerMm);
                         // Rod overlapping the square merges the blobs.
                         scene.addRect(24.0 + 10.0 * pxPerMm, 24.0 + 10.0 * pxPerMm, 40.0 * pxPerMm,
                                       6.0 * pxPerMm, deg(15.0));
                     },
                     AnalyzeError::NoReferenceTarget, std::nullopt, std::nullopt, 0.0, 0.0,
                     "merged blob no longer passes the square gates"});

    // Honest-refusal cases at the classifier layer.
    cases.push_back({"compact block without bore", "measurement", 0.25,
                     [](SyntheticScene& scene, double pxPerMm) {
                         addReference(scene, pxPerMm);
                         scene.addRect(400.0, 300.0, 15.0 * pxPerMm, 13.0 * pxPerMm, 0.0);
                     },
                     std::nullopt, FastenerClass::Unknown, std::nullopt, 0.0, 0.0,
                     "no bore, compact: honestly unknown"});
    cases.push_back({"rod with off-table width (15 mm)", "measurement", 0.5,
                     [](SyntheticScene& scene, double pxPerMm) {
                         addReference(scene, pxPerMm);
                         scene.addRect(400.0, 300.0, 80.0 * pxPerMm, 15.0 * pxPerMm, deg(30.0));
                     },
                     std::nullopt, FastenerClass::BoltOrScrew, std::nullopt, 80.0, 15.0,
                     "no ISO shaft nominal within tolerance; nominal correctly withheld"});

    return cases;
}

}  // namespace

ValidationRun runValidation() {
    ValidationRun run;
    const CalibrationSpec spec{20.0};

    double sumLenErr = 0.0;
    double sumWidErr = 0.0;
    std::size_t lenSamples = 0;
    std::size_t widSamples = 0;

    for (const auto& caseSpec : buildCases()) {
        CaseResult result;
        result.name = caseSpec.name;
        result.kind = caseSpec.kind;
        result.expectedError = caseSpec.expectedError;
        result.expectedClass = caseSpec.expectedClass;
        result.expectedNominal = caseSpec.expectedNominal;
        result.truthLengthMm = caseSpec.truthLengthMm;
        result.truthWidthMm = caseSpec.truthWidthMm;
        result.note = caseSpec.note;

        SyntheticScene scene;
        caseSpec.build(scene, 1.0 / caseSpec.mmPerPx);
        const auto analyzed = vision::analyzeFrame(scene.frame(), spec);

        if (!analyzed.ok()) {
            result.actualError = analyzed.error;
            result.behaved = caseSpec.expectedError && *caseSpec.expectedError == analyzed.error;
        } else if (caseSpec.expectedError) {
            result.behaved = false;  // expected a refusal, got an analysis
        } else {
            const auto classified = ai::classify(*analyzed.analysis);
            result.actualClass = classified.fastenerClass;
            if (classified.nominal) result.actualNominal = classified.nominal->designation;
            result.measuredLengthMm = analyzed.analysis->subjectLengthMm;
            result.measuredWidthMm = analyzed.analysis->subjectWidthMm;

            const bool classOk =
                caseSpec.expectedClass && *caseSpec.expectedClass == classified.fastenerClass;
            const bool nominalOk = result.actualNominal == caseSpec.expectedNominal;
            result.behaved = classOk && nominalOk;

            ++run.summary.classApplicable;
            if (classOk) ++run.summary.classHits;
            if (caseSpec.expectedNominal) {
                ++run.summary.nominalApplicable;
                if (nominalOk) ++run.summary.nominalHits;
            }
            if (caseSpec.truthLengthMm > 0.0) {
                const auto err = std::abs(result.measuredLengthMm - caseSpec.truthLengthMm);
                sumLenErr += err;
                run.summary.maxAbsLengthErrMm = std::max(run.summary.maxAbsLengthErrMm, err);
                ++lenSamples;
            }
            if (caseSpec.truthWidthMm > 0.0) {
                const auto err = std::abs(result.measuredWidthMm - caseSpec.truthWidthMm);
                sumWidErr += err;
                run.summary.maxAbsWidthErrMm = std::max(run.summary.maxAbsWidthErrMm, err);
                ++widSamples;
            }
        }

        if (caseSpec.kind == "failure") {
            ++run.summary.failureCases;
            if (result.behaved) ++run.summary.failureBehaved;
        }
        ++run.summary.cases;
        if (result.behaved) ++run.summary.behaved;
        run.results.push_back(std::move(result));
    }

    if (lenSamples > 0) run.summary.meanAbsLengthErrMm = sumLenErr / lenSamples;
    if (widSamples > 0) run.summary.meanAbsWidthErrMm = sumWidErr / widSamples;
    return run;
}

namespace {

std::string percent(std::size_t hits, std::size_t total) {
    if (total == 0) return "n/a";
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%.0f%%", 100.0 * hits / total);
    return buffer;
}

std::string mm(double value) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%.2f", value);
    return buffer;
}

std::string classText(const std::optional<ai::FastenerClass>& value) {
    return value ? std::string(ai::to_string(*value)) : "-";
}

std::string errorText(const std::optional<vision::AnalyzeError>& value) {
    return value ? std::string(vision::to_string(*value)) : "-";
}

}  // namespace

std::string formatReport(const ValidationRun& run) {
    const auto& s = run.summary;
    std::string out;
    out += "# Scout validation report\n\n";
    out +=
        "Generated by `tools/scout_validation` against `vision.scout_analyzer.v2` +\n"
        "`ai.fastener_classifier.v2` on the synthetic ground-truth battery. Regenerate\n"
        "with `scout_validation REPORT.md`; the output is deterministic for a given\n"
        "build. Failure cases PASS when the pipeline refuses for the expected reason.\n\n";

    out += "## Summary\n\n";
    out += "| Metric | Value |\n|---|---|\n";
    out += "| Cases behaving as specified | " + std::to_string(s.behaved) + "/" +
           std::to_string(s.cases) + " |\n";
    out += "| Classification hit rate | " + percent(s.classHits, s.classApplicable) + " (" +
           std::to_string(s.classHits) + "/" + std::to_string(s.classApplicable) + ") |\n";
    out += "| Nominal-size hit rate | " + percent(s.nominalHits, s.nominalApplicable) + " (" +
           std::to_string(s.nominalHits) + "/" + std::to_string(s.nominalApplicable) + ") |\n";
    out += "| Intentional failure cases behaving | " + std::to_string(s.failureBehaved) + "/" +
           std::to_string(s.failureCases) + " |\n";
    out += "| Length error (mean / max) | " + mm(s.meanAbsLengthErrMm) + " / " +
           mm(s.maxAbsLengthErrMm) + " mm |\n";
    out += "| Width error (mean / max) | " + mm(s.meanAbsWidthErrMm) + " / " +
           mm(s.maxAbsWidthErrMm) + " mm |\n\n";

    out += "## Cases\n\n";
    out +=
        "| Case | Expected | Actual | Measured (mm) | Truth (mm) | OK | Note |\n"
        "|---|---|---|---|---|---|---|\n";
    for (const auto& r : run.results) {
        std::string expected;
        std::string actual;
        if (r.expectedError) {
            expected = errorText(r.expectedError);
            actual = r.actualError ? errorText(r.actualError)
                                   : "analysis succeeded (class " + classText(r.actualClass) + ")";
        } else {
            expected = classText(r.expectedClass);
            if (r.expectedNominal) expected += " " + *r.expectedNominal;
            actual = r.actualError ? errorText(r.actualError) : classText(r.actualClass);
            if (r.actualNominal) actual += " " + *r.actualNominal;
        }
        const std::string measured =
            r.measuredLengthMm > 0.0 ? mm(r.measuredLengthMm) + " x " + mm(r.measuredWidthMm) : "-";
        const std::string truth =
            r.truthLengthMm > 0.0 ? mm(r.truthLengthMm) + " x " + mm(r.truthWidthMm)
                                  : (r.truthWidthMm > 0.0 ? "width " + mm(r.truthWidthMm) : "-");
        out += "| " + r.name + " | " + expected + " | " + actual + " | " + measured + " | " +
               truth + " | " + (r.behaved ? "PASS" : "FAIL") + " | " + r.note + " |\n";
    }

    out +=
        "\n## Known limitations this battery documents\n\n"
        "- A washer's outer diameter reads through the across-flats table as a\n"
        "  conditional nominal (valid only if the part is a nut); nut_vs_washer\n"
        "  stays unresolved until a side profile lands.\n"
        "- Bolt nominals match the shank only; a head-dominated top-down outline\n"
        "  will overstate the nominal (see ai.fastener_classifier.v2 notes).\n"
        "- The width measurement (minimum support width, 1-deg sweep) is\n"
        "  rotation-invariant; the earlier principal-axis extent overestimated\n"
        "  rotated hexagons and is retired as of vision.scout_analyzer.v2.\n";
    return out;
}

}  // namespace platypus::validation
