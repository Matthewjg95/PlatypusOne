#include "platypus/session/ObjectProfile.hpp"

namespace platypus::session {

const QuestionRule* ObjectProfile::rule(const std::string& field) const {
    for (const auto& q : questions)
        if (q.field == field) return &q;
    return nullptr;
}

ObjectProfile fastenerProfile() {
    ObjectProfile p;
    p.id = "fastener";
    p.name = "Fastener";
    p.measurands = {{"subject_length", "length", "mm"}, {"subject_width", "width", "mm"}};
    p.requiredRepeats = 3;
    p.repeatToleranceMm = 0.5;
    p.agreementClaims = {"fastener_class", "nominal_size"};
    p.questions = {
        // Answerable by another top-down capture with this build's analyzers.
        {"fastener_class", "a top-down capture with the part flat and fully in view", true},
        {"nominal_size", "a top-down capture with the part flat and fully in view", true},
        // Need side-view analysis this build does not have yet.
        {"thread_pitch", "a side-on view with the thread against a plain background", false},
        {"bolt_vs_screw", "a side-on view of the head", false},
        {"nut_vs_washer", "a side-on view to measure thickness", false},
    };
    p.caveats = {
        "Measured as a silhouette at the paper plane: features standing above it, such as a "
        "screw head, read larger by parallax.",
        "Low-angle light adds a shadow halo that widens silhouettes; a backlight removes it.",
        "Width is the narrowest band containing the whole silhouette - for a headed screw that "
        "is the head, not the shaft.",
    };
    return p;
}

}  // namespace platypus::session
