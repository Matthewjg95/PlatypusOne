// SettingsStore contract tests: typed round-trips, defaults, atomic persist,
// deterministic output, damaged-file degradation, and input validation.
#include <platypus/filesystem/SettingsStore.hpp>

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

namespace fs = std::filesystem;
using platypus::filesystem::SettingsStore;

struct ScratchFile {
    ScratchFile() : path(fs::temp_directory_path() / "platypus-test-settings" / "settings.conf") {
        std::error_code ec;
        fs::remove_all(path.parent_path(), ec);
    }
    ~ScratchFile() {
        std::error_code ec;
        fs::remove_all(path.parent_path(), ec);
    }
    fs::path path;
};

std::string readFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

void test_round_trip() {
    ScratchFile scratch;
    SettingsStore store(scratch.path);
    assert(store.load().ok());  // missing file = empty store
    assert(store.size() == 0);

    assert(store.set("brightness", 0.8).ok());
    assert(store.set("scout.reference_side_mm", 20.0).ok());
    assert(store.set("boot.count", std::int64_t{7}).ok());
    assert(store.set("sim.geometry", std::string("800x480")).ok());
    assert(store.set("audio.muted", true).ok());
    assert(store.dirty());
    assert(store.save().ok());
    assert(!store.dirty());

    SettingsStore reloaded(scratch.path);
    assert(reloaded.load().ok());
    assert(reloaded.size() == 5);
    assert(reloaded.getDouble("brightness", 0.0) == 0.8);
    assert(reloaded.getDouble("scout.reference_side_mm", 0.0) == 20.0);
    assert(reloaded.getInt("boot.count", 0) == 7);
    assert(reloaded.getString("sim.geometry", "") == "800x480");
    assert(reloaded.getBool("audio.muted", false));
}

void test_defaults_and_type_mismatch() {
    ScratchFile scratch;
    SettingsStore store(scratch.path);
    assert(store.getBool("missing", true));
    assert(store.getInt("missing", 42) == 42);
    assert(store.getString("missing", "fallback") == "fallback");

    assert(store.set("count", std::int64_t{5}).ok());
    // Reading an int as a double widens; reading it as a string falls back.
    assert(store.getDouble("count", 0.0) == 5.0);
    assert(store.getString("count", "fallback") == "fallback");
}

void test_deterministic_and_atomic() {
    ScratchFile scratch;
    SettingsStore store(scratch.path);
    assert(store.set("zeta", std::int64_t{1}).ok());
    assert(store.set("alpha", std::int64_t{2}).ok());
    assert(store.save().ok());
    const auto first = readFile(scratch.path);
    assert(first.find("alpha=i:2") < first.find("zeta=i:1"));  // sorted keys
    assert(store.save().ok());
    assert(readFile(scratch.path) == first);  // byte-identical
    assert(!fs::exists(scratch.path.string() + ".tmp"));

    // erase() persists on the next save.
    assert(store.erase("zeta").ok());
    assert(!store.erase("zeta").ok());
    assert(store.save().ok());
    assert(readFile(scratch.path).find("zeta") == std::string::npos);
}

void test_damaged_file_degrades() {
    ScratchFile scratch;
    fs::create_directories(scratch.path.parent_path());
    std::ofstream out(scratch.path, std::ios::binary);
    out << "# comment survives\n"
        << "good=i:9\n"
        << "no-equals-line\n"
        << "bad-type=x:1\n"
        << "bad-int=i:12abc\n"
        << "=b:true\n"
        << "trailing=s:text with = sign\n";
    out.close();

    SettingsStore store(scratch.path);
    assert(store.load().ok());
    assert(store.getInt("good", 0) == 9);
    assert(store.getString("trailing", "") == "text with = sign");
    assert(store.size() == 2);
    assert(store.skippedLines() == 4);
}

void test_validation() {
    ScratchFile scratch;
    SettingsStore store(scratch.path);
    assert(!store.set("", true).ok());
    assert(!store.set("has=equals", true).ok());
    assert(!store.set("has\nnewline", true).ok());
    assert(!store.set("key", std::string("line\nbreak")).ok());
    assert(store.set("key", std::string("spaces and = are fine")).ok());
}

}  // namespace

void test_settings_store() {
    test_round_trip();
    test_defaults_and_type_mismatch();
    test_deterministic_and_atomic();
    test_damaged_file_degrades();
    test_validation();
    std::puts("test_settings_store: OK");
}
