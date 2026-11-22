// PlatypusOS services — object profiles for Scout sessions.
//
// What a session measures, which questions it knows a capture cannot answer,
// and which view would answer them — as data, per object class. Fasteners are
// the first profile, not the design: a PCB, bracket or turned part is a new
// profile, not a new session engine
// (docs/architecture/SCOUT_SESSIONS_AND_BEYOND.md §2).
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace platypus::session {

/// One quantity repeatability is judged on, read by claim *name* from each
/// capture's derived[] claims — names are the contract's semantic keys, ids
/// are per-record.
struct Measurand {
    std::string claimName;  ///< e.g. "subject_length"
    std::string label;      ///< operator-facing, e.g. "length"
    std::string unit;       ///< e.g. "mm"
};

/// A question the profile knows a top-down capture may leave open, the view
/// that would answer it, and whether this build can analyze that view. The
/// flag is the honesty mechanism: a session may *request* a side view before
/// any side-view analyzer exists, but must never imply it resolved anything.
struct QuestionRule {
    std::string field;          ///< matches unresolved[].name, e.g. "thread_pitch"
    std::string resolvingView;  ///< the capture that would answer it
    bool analyzerInBuild = false;
};

struct ObjectProfile {
    std::string id;    ///< stable, recorded in session.json, e.g. "fastener"
    std::string name;  ///< operator-facing
    std::vector<Measurand> measurands;
    /// Accepted captures needed before the measurement counts as repeatable,
    /// and the largest spread among them that still does.
    std::size_t requiredRepeats = 3;
    double repeatToleranceMm = 0.5;
    /// Inferred claim names that must agree across accepted captures before
    /// the model is satisfied, e.g. "fastener_class".
    std::vector<std::string> agreementClaims;
    std::vector<QuestionRule> questions;
    /// Known measurement limits carried into every export, so an artifact is
    /// never mistaken for ground truth.
    std::vector<std::string> caveats;

    [[nodiscard]] const QuestionRule* rule(const std::string& field) const;
};

/// Bolts, screws, nuts and washers seen top-down beside the 20 mm reference.
[[nodiscard]] ObjectProfile fastenerProfile();

}  // namespace platypus::session
