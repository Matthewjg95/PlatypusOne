// PlatypusOS services — minimal JSON value model for the observation contract.
//
// Deliberately small and dependency-free (third_party/ stays empty): a value
// tree, a strict serializer, and a strict parser. Scoped to what the
// Engineering Observation contract needs — this is not a general-purpose JSON
// library and should not grow into one without an ADR.
//
// Threading: values are plain data; no shared state. Safe to use from any
// thread that owns the value.
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace platypus::observation::json {

class Value;

using Array = std::vector<Value>;
/// Insertion-ordered: the observation serializer emits fields in contract
/// order and that order must survive serialization. The parser rejects
/// duplicate keys, so lookup ambiguity cannot arise.
using Object = std::vector<std::pair<std::string, Value>>;

/// One JSON value. Numbers are IEEE doubles (the contract's values are
/// physical quantities and confidences; integer identity is not required).
class Value {
public:
    Value() : data_(nullptr) {}                       // null
    Value(std::nullptr_t) : data_(nullptr) {}
    Value(bool b) : data_(b) {}
    Value(double d) : data_(d) {}
    Value(int i) : data_(static_cast<double>(i)) {}
    Value(const char* s) : data_(std::string(s)) {}
    Value(std::string s) : data_(std::move(s)) {}
    Value(Array a) : data_(std::move(a)) {}
    Value(Object o) : data_(std::move(o)) {}

    [[nodiscard]] bool isNull() const noexcept { return std::holds_alternative<std::nullptr_t>(data_); }
    [[nodiscard]] bool isBool() const noexcept { return std::holds_alternative<bool>(data_); }
    [[nodiscard]] bool isNumber() const noexcept { return std::holds_alternative<double>(data_); }
    [[nodiscard]] bool isString() const noexcept { return std::holds_alternative<std::string>(data_); }
    [[nodiscard]] bool isArray() const noexcept { return std::holds_alternative<Array>(data_); }
    [[nodiscard]] bool isObject() const noexcept { return std::holds_alternative<Object>(data_); }

    [[nodiscard]] bool asBool() const { return std::get<bool>(data_); }
    [[nodiscard]] double asNumber() const { return std::get<double>(data_); }
    [[nodiscard]] const std::string& asString() const { return std::get<std::string>(data_); }
    [[nodiscard]] const Array& asArray() const { return std::get<Array>(data_); }
    [[nodiscard]] Array& asArray() { return std::get<Array>(data_); }
    [[nodiscard]] const Object& asObject() const { return std::get<Object>(data_); }
    [[nodiscard]] Object& asObject() { return std::get<Object>(data_); }

    /// Object member lookup; nullptr when absent or not an object.
    [[nodiscard]] const Value* find(std::string_view key) const {
        if (!isObject()) return nullptr;
        for (const auto& [k, v] : asObject())
            if (k == key) return &v;
        return nullptr;
    }

    bool operator==(const Value& other) const = default;

private:
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> data_;
};

/// Serializes with two-space indentation, "\n" line ends, keys in map order.
/// Escapes ", \, and control characters (\uXXXX for controls without a short
/// escape). Numbers use the shortest round-trippable representation.
[[nodiscard]] std::string serialize(const Value& value);

struct ParseOutcome {
    std::optional<Value> value;   ///< set on success
    std::string error;            ///< human-readable reason on failure
    std::size_t offset = 0;       ///< byte offset of the failure
    [[nodiscard]] bool ok() const noexcept { return value.has_value(); }
};

/// Strict parser: full RFC 8259 value grammar with \uXXXX escapes (including
/// surrogate pairs), a nesting-depth cap, and no trailing-garbage tolerance.
[[nodiscard]] ParseOutcome parse(std::string_view text);

}  // namespace platypus::observation::json
