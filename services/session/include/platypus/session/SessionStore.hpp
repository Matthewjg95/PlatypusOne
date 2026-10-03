// PlatypusOS services — Scout session persistence and CAD export.
//
//   <root>/<session_id>/session.json    written after every capture (open),
//                                       rewritten on finish (closed)
//   <root>/<session_id>/summary.md      human-readable handoff note
//   <root>/<session_id>/outline.json    mesh2cad Outline Forge sketch
//   <root>/<session_id>/outline.dxf     Fusion 360 / KiCad sketch, mm
//   <root>/<session_id>/source.png      the representative capture
//
// Like the observation store: no database, the folder is the record. Files
// are written to a temporary name and renamed into place, so a crash never
// leaves a half-written session.json.
#pragma once

#include <platypus/session/Session.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace platypus::session {

struct ArtifactRef {
    std::string id;    ///< e.g. "outline-forge"
    std::string kind;  ///< MIME-ish, e.g. "application/dxf"
    std::string path;  ///< relative to the session directory
    std::string note;  ///< what it is for
};

/// session.json contents. `closed` is set on finish; `closedUtc` and
/// `artifacts` only matter then.
[[nodiscard]] std::string sessionJson(const ObjectSession& session,
                                      std::optional<CloseReason> closed = std::nullopt,
                                      const std::string& closedUtc = {},
                                      const std::vector<ArtifactRef>& artifacts = {});

/// summary.md contents: what was measured, how confidently, what is still
/// open, what the artifacts are, and the measurement caveats.
[[nodiscard]] std::string summaryMarkdown(const ObjectSession& session, CloseReason closed,
                                          const std::string& closedUtc,
                                          const std::vector<ArtifactRef>& artifacts);

struct FinishResult {
    std::filesystem::path directory;
    std::vector<ArtifactRef> artifacts;
    std::string error;  ///< empty on success
    [[nodiscard]] bool ok() const noexcept { return error.empty(); }
};

class SessionStore {
   public:
    explicit SessionStore(std::filesystem::path root);

    /// First free "sess-NNNN" id under the root.
    [[nodiscard]] std::string nextSessionId() const;

    /// Writes session.json for a session still in progress.
    bool save(const ObjectSession& session, std::string* error = nullptr) const;

    /// Writes the CAD artifacts for the representative capture, summary.md,
    /// and the closed session.json.
    [[nodiscard]] FinishResult finish(const ObjectSession& session, CloseReason reason,
                                      const std::string& closedUtc) const;

    [[nodiscard]] const std::filesystem::path& root() const noexcept { return root_; }

   private:
    std::filesystem::path root_;
};

}  // namespace platypus::session
