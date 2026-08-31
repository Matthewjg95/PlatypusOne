// PlatypusOS services — typed key/value settings persistence
// (ROADMAP: filesystem/settings-store).
//
// One flat, line-oriented text file — deliberately not JSON: settings need no
// nesting, and `name=type:value` lines diff cleanly and survive hand edits on
// the device. Values are typed (bool / int / double / string); readers ask
// with a default so missing keys never branch error paths through app code.
//
//   brightness=f:0.8
//   scout.reference_side_mm=f:20
//   sim.geometry=s:800x480
//
// Mutations are in-memory; save() persists atomically (temp file + rename) so
// a power cut mid-write never corrupts the previous state. Loading skips
// malformed lines rather than failing: a damaged file degrades to defaults,
// key by key.
#pragma once

#include <platypus/hal/Result.hpp>

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace platypus::filesystem {

using SettingValue = std::variant<bool, std::int64_t, double, std::string>;

class SettingsStore {
   public:
    /// The file need not exist yet; load() on a missing file is an empty
    /// store, not an error.
    explicit SettingsStore(std::filesystem::path file);

    /// Replaces in-memory state with the file's contents. Malformed lines are
    /// skipped and counted in skippedLines().
    hal::Status load();

    /// Atomic persist (temp + rename), keys sorted so output is deterministic.
    hal::Status save();

    /// Keys: nonempty, no '=', no newlines. String values: no newlines.
    hal::Status set(std::string_view key, SettingValue value);
    hal::Status erase(std::string_view key);

    [[nodiscard]] std::optional<SettingValue> get(std::string_view key) const;
    [[nodiscard]] bool getBool(std::string_view key, bool fallback) const;
    [[nodiscard]] std::int64_t getInt(std::string_view key, std::int64_t fallback) const;
    [[nodiscard]] double getDouble(std::string_view key, double fallback) const;
    [[nodiscard]] std::string getString(std::string_view key, std::string_view fallback) const;

    [[nodiscard]] std::size_t size() const noexcept { return values_.size(); }
    [[nodiscard]] bool dirty() const noexcept { return dirty_; }
    [[nodiscard]] std::size_t skippedLines() const noexcept { return skippedLines_; }
    [[nodiscard]] const std::filesystem::path& file() const noexcept { return file_; }

   private:
    std::filesystem::path file_;
    std::map<std::string, SettingValue, std::less<>> values_;
    bool dirty_ = false;
    std::size_t skippedLines_ = 0;
};

}  // namespace platypus::filesystem
