#include "platypus/observation/Json.hpp"

#include <charconv>
#include <cstdint>
#include <cstdio>
#include <format>

namespace platypus::observation::json {

// ---------------------------------------------------------------------------
// Serialization
// ---------------------------------------------------------------------------
namespace {

void appendEscaped(std::string& out, const std::string& s) {
    out += '"';
    for (const char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    out += std::format("\\u{:04x}", static_cast<unsigned char>(c));
                } else {
                    out += c;  // UTF-8 bytes pass through unescaped
                }
        }
    }
    out += '"';
}

void appendNumber(std::string& out, double d) {
    // std::format("{}") yields the shortest representation that round-trips.
    // JSON has no Inf/NaN; the contract never produces them, but clamp to
    // null rather than emitting invalid JSON if one slips through.
    if (d != d || d > 1.7976931348623157e308 || d < -1.7976931348623157e308) {
        out += "null";
        return;
    }
    out += std::format("{}", d);
}

void serializeInto(std::string& out, const Value& v, int indent, int depth) {
    const std::string pad(static_cast<std::size_t>(indent) * depth, ' ');
    const std::string padIn(static_cast<std::size_t>(indent) * (depth + 1), ' ');

    if (v.isNull()) { out += "null"; return; }
    if (v.isBool()) { out += v.asBool() ? "true" : "false"; return; }
    if (v.isNumber()) { appendNumber(out, v.asNumber()); return; }
    if (v.isString()) { appendEscaped(out, v.asString()); return; }

    if (v.isArray()) {
        const auto& arr = v.asArray();
        if (arr.empty()) { out += "[]"; return; }
        out += "[\n";
        for (std::size_t i = 0; i < arr.size(); ++i) {
            out += padIn;
            serializeInto(out, arr[i], indent, depth + 1);
            if (i + 1 < arr.size()) out += ',';
            out += '\n';
        }
        out += pad + "]";
        return;
    }

    const auto& obj = v.asObject();
    if (obj.empty()) { out += "{}"; return; }
    out += "{\n";
    std::size_t i = 0;
    for (const auto& [key, member] : obj) {
        out += padIn;
        appendEscaped(out, key);
        out += ": ";
        serializeInto(out, member, indent, depth + 1);
        if (++i < obj.size()) out += ',';
        out += '\n';
    }
    out += pad + "}";
}

}  // namespace

std::string serialize(const Value& value) {
    std::string out;
    serializeInto(out, value, 2, 0);
    out += '\n';
    return out;
}

// ---------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------
namespace {

constexpr int kMaxDepth = 64;

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    ParseOutcome run() {
        skipWs();
        Value v;
        if (!parseValue(v, 0)) return fail();
        skipWs();
        if (pos_ != text_.size()) return fail("trailing characters after value");
        ParseOutcome outcome;
        outcome.value = std::move(v);
        return outcome;
    }

private:
    ParseOutcome fail(std::string message = {}) {
        ParseOutcome outcome;
        outcome.error = message.empty() ? error_ : std::move(message);
        if (outcome.error.empty()) outcome.error = "malformed JSON";
        outcome.offset = pos_;
        return outcome;
    }

    bool err(std::string message) {
        if (error_.empty()) error_ = std::move(message);
        return false;
    }

    void skipWs() {
        while (pos_ < text_.size()) {
            const char c = text_[pos_];
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') break;
            ++pos_;
        }
    }

    [[nodiscard]] bool atEnd() const { return pos_ >= text_.size(); }
    [[nodiscard]] char peek() const { return text_[pos_]; }

    bool consume(char expected) {
        if (atEnd() || text_[pos_] != expected)
            return err(std::string("expected '") + expected + "'");
        ++pos_;
        return true;
    }

    bool literal(std::string_view word, Value out, Value& target) {
        if (text_.substr(pos_, word.size()) != word) return err("invalid literal");
        pos_ += word.size();
        target = std::move(out);
        return true;
    }

    bool parseValue(Value& out, int depth) {
        if (depth > kMaxDepth) return err("nesting too deep");
        if (atEnd()) return err("unexpected end of input");
        switch (peek()) {
            case '{': return parseObject(out, depth);
            case '[': return parseArray(out, depth);
            case '"': {
                std::string s;
                if (!parseString(s)) return false;
                out = Value(std::move(s));
                return true;
            }
            case 't': return literal("true", Value(true), out);
            case 'f': return literal("false", Value(false), out);
            case 'n': return literal("null", Value(nullptr), out);
            default:  return parseNumber(out);
        }
    }

    bool parseObject(Value& out, int depth) {
        if (!consume('{')) return false;
        Object obj;
        skipWs();
        if (!atEnd() && peek() == '}') { ++pos_; out = Value(std::move(obj)); return true; }
        for (;;) {
            skipWs();
            std::string key;
            if (!parseString(key)) return false;
            skipWs();
            if (!consume(':')) return false;
            skipWs();
            Value member;
            if (!parseValue(member, depth + 1)) return false;
            for (const auto& [existing, unused] : obj)
                if (existing == key) return err("duplicate object key: " + key);
            obj.emplace_back(std::move(key), std::move(member));
            skipWs();
            if (atEnd()) return err("unterminated object");
            if (peek() == ',') { ++pos_; continue; }
            if (peek() == '}') { ++pos_; break; }
            return err("expected ',' or '}' in object");
        }
        out = Value(std::move(obj));
        return true;
    }

    bool parseArray(Value& out, int depth) {
        if (!consume('[')) return false;
        Array arr;
        skipWs();
        if (!atEnd() && peek() == ']') { ++pos_; out = Value(std::move(arr)); return true; }
        for (;;) {
            skipWs();
            Value element;
            if (!parseValue(element, depth + 1)) return false;
            arr.push_back(std::move(element));
            skipWs();
            if (atEnd()) return err("unterminated array");
            if (peek() == ',') { ++pos_; continue; }
            if (peek() == ']') { ++pos_; break; }
            return err("expected ',' or ']' in array");
        }
        out = Value(std::move(arr));
        return true;
    }

    bool parseHex4(std::uint32_t& out) {
        if (pos_ + 4 > text_.size()) return err("truncated \\u escape");
        out = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text_[pos_++];
            out <<= 4;
            if (c >= '0' && c <= '9') out |= static_cast<std::uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') out |= static_cast<std::uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') out |= static_cast<std::uint32_t>(c - 'A' + 10);
            else return err("invalid hex digit in \\u escape");
        }
        return true;
    }

    static void appendUtf8(std::string& s, std::uint32_t cp) {
        if (cp < 0x80) {
            s += static_cast<char>(cp);
        } else if (cp < 0x800) {
            s += static_cast<char>(0xC0 | (cp >> 6));
            s += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            s += static_cast<char>(0xE0 | (cp >> 12));
            s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            s += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            s += static_cast<char>(0xF0 | (cp >> 18));
            s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            s += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    bool parseString(std::string& out) {
        if (!consume('"')) return false;
        out.clear();
        while (!atEnd()) {
            const char c = text_[pos_++];
            if (c == '"') return true;
            if (static_cast<unsigned char>(c) < 0x20)
                return err("unescaped control character in string");
            if (c != '\\') { out += c; continue; }

            if (atEnd()) return err("truncated escape");
            const char esc = text_[pos_++];
            switch (esc) {
                case '"':  out += '"'; break;
                case '\\': out += '\\'; break;
                case '/':  out += '/'; break;
                case 'b':  out += '\b'; break;
                case 'f':  out += '\f'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                case 'u': {
                    std::uint32_t cp = 0;
                    if (!parseHex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF) {  // high surrogate
                        if (pos_ + 2 > text_.size() || text_[pos_] != '\\' ||
                            text_[pos_ + 1] != 'u')
                            return err("high surrogate not followed by \\u");
                        pos_ += 2;
                        std::uint32_t low = 0;
                        if (!parseHex4(low)) return false;
                        if (low < 0xDC00 || low > 0xDFFF)
                            return err("invalid low surrogate");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        return err("unexpected low surrogate");
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default: return err("unknown escape character");
            }
        }
        return err("unterminated string");
    }

    bool parseNumber(Value& out) {
        const std::size_t start = pos_;
        if (!atEnd() && peek() == '-') ++pos_;
        while (!atEnd() && ((peek() >= '0' && peek() <= '9') || peek() == '.' ||
                            peek() == 'e' || peek() == 'E' || peek() == '+' ||
                            peek() == '-'))
            ++pos_;
        if (pos_ == start) return err("invalid value");
        double d = 0;
        const auto* first = text_.data() + start;
        const auto* last = text_.data() + pos_;
        const auto [end, ec] = std::from_chars(first, last, d);
        if (ec != std::errc{} || end != last) return err("invalid number");
        out = Value(d);
        return true;
    }

    std::string_view text_;
    std::size_t pos_ = 0;
    std::string error_;
};

}  // namespace

ParseOutcome parse(std::string_view text) { return Parser(text).run(); }

}  // namespace platypus::observation::json
