#include "platypus/filesystem/SettingsStore.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>

namespace platypus::filesystem {

namespace {

using hal::Error;

bool validKey(std::string_view key) {
    return !key.empty() && key.find_first_of("=\r\n") == std::string_view::npos;
}

bool validValue(const SettingValue& value) {
    if (const auto* text = std::get_if<std::string>(&value))
        return text->find_first_of("\r\n") == std::string::npos;
    return true;
}

/// Doubles round-trip through %.17g; trailing formatting is what it is —
/// determinism matters more than prettiness here.
std::string encode(const SettingValue& value) {
    if (const auto* flag = std::get_if<bool>(&value)) return *flag ? "b:true" : "b:false";
    if (const auto* whole = std::get_if<std::int64_t>(&value)) return "i:" + std::to_string(*whole);
    if (const auto* real = std::get_if<double>(&value)) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "f:%.17g", *real);
        return buffer;
    }
    return "s:" + std::get<std::string>(value);
}

std::optional<SettingValue> decode(std::string_view text) {
    if (text.size() < 2 || text[1] != ':') return std::nullopt;
    const auto payload = text.substr(2);
    switch (text[0]) {
        case 'b':
            if (payload == "true") return SettingValue{true};
            if (payload == "false") return SettingValue{false};
            return std::nullopt;
        case 'i': {
            errno = 0;
            char* end = nullptr;
            const std::string owned(payload);
            const auto parsed = std::strtoll(owned.c_str(), &end, 10);
            if (errno != 0 || end != owned.c_str() + owned.size() || owned.empty())
                return std::nullopt;
            return SettingValue{static_cast<std::int64_t>(parsed)};
        }
        case 'f': {
            errno = 0;
            char* end = nullptr;
            const std::string owned(payload);
            const auto parsed = std::strtod(owned.c_str(), &end);
            if (errno != 0 || end != owned.c_str() + owned.size() || owned.empty())
                return std::nullopt;
            return SettingValue{parsed};
        }
        case 's':
            return SettingValue{std::string(payload)};
        default:
            return std::nullopt;
    }
}

}  // namespace

SettingsStore::SettingsStore(std::filesystem::path file) : file_(std::move(file)) {}

hal::Status SettingsStore::load() {
    values_.clear();
    skippedLines_ = 0;
    dirty_ = false;

    std::error_code ec;
    if (!std::filesystem::exists(file_, ec)) return {};  // missing file = empty store

    std::ifstream in(file_, std::ios::binary);
    if (!in) return Error::IoFailure;

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line.front() == '#') continue;
        const auto equals = line.find('=');
        if (equals == std::string::npos || equals == 0) {
            ++skippedLines_;
            continue;
        }
        auto decoded = decode(std::string_view(line).substr(equals + 1));
        if (!decoded) {
            ++skippedLines_;
            continue;
        }
        values_.insert_or_assign(line.substr(0, equals), std::move(*decoded));
    }
    return {};
}

hal::Status SettingsStore::save() {
    std::error_code ec;
    if (file_.has_parent_path()) {
        std::filesystem::create_directories(file_.parent_path(), ec);
        if (ec) return Error::IoFailure;
    }

    const auto temp = file_.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) return Error::IoFailure;
        out << "# PlatypusOS settings — name=type:value; types b/i/f/s\n";
        for (const auto& [key, value] : values_)
            out << key << '=' << encode(value) << '\n';
        if (!out.good()) return Error::IoFailure;
    }

    std::filesystem::rename(temp, file_, ec);
    if (ec) {
        std::filesystem::remove(temp, ec);
        return Error::IoFailure;
    }
    dirty_ = false;
    return {};
}

hal::Status SettingsStore::set(std::string_view key, SettingValue value) {
    if (!validKey(key) || !validValue(value)) return Error::InvalidArgument;
    values_.insert_or_assign(std::string(key), std::move(value));
    dirty_ = true;
    return {};
}

hal::Status SettingsStore::erase(std::string_view key) {
    const auto it = values_.find(key);
    if (it == values_.end()) return Error::OutOfRange;
    values_.erase(it);
    dirty_ = true;
    return {};
}

std::optional<SettingValue> SettingsStore::get(std::string_view key) const {
    const auto it = values_.find(key);
    if (it == values_.end()) return std::nullopt;
    return it->second;
}

bool SettingsStore::getBool(std::string_view key, bool fallback) const {
    const auto value = get(key);
    if (!value) return fallback;
    if (const auto* typed = std::get_if<bool>(&*value)) return *typed;
    return fallback;
}

std::int64_t SettingsStore::getInt(std::string_view key, std::int64_t fallback) const {
    const auto value = get(key);
    if (!value) return fallback;
    if (const auto* typed = std::get_if<std::int64_t>(&*value)) return *typed;
    return fallback;
}

double SettingsStore::getDouble(std::string_view key, double fallback) const {
    const auto value = get(key);
    if (!value) return fallback;
    if (const auto* typed = std::get_if<double>(&*value)) return *typed;
    // An integer written where a double is read is a compatible widening.
    if (const auto* whole = std::get_if<std::int64_t>(&*value)) return static_cast<double>(*whole);
    return fallback;
}

std::string SettingsStore::getString(std::string_view key, std::string_view fallback) const {
    const auto value = get(key);
    if (!value) return std::string(fallback);
    if (const auto* typed = std::get_if<std::string>(&*value)) return *typed;
    return std::string(fallback);
}

}  // namespace platypus::filesystem
