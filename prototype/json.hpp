#pragma once
// ---------------------------------------------------------------------------
// json.hpp  —  可行性验证原型用的最小 JSON 库（无第三方依赖，仅演示用）
//
// 目的：证明 "多文件 JSON 存储" 的数据结构在 MSVC / C++20 下可行，且
//       序列化结果与 Python / Qt (QJsonDocument) 互操作。
// 正式工程中将由 QJsonDocument / QJsonObject 替代本文件。
// ---------------------------------------------------------------------------
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace jx {

struct Value {
    enum class T : int { Null = 0, Bool, Int, Real, Str, Arr, Obj };

    T t = T::Null;
    bool b = false;
    long long i = 0;
    double d = 0.0;
    std::string s;
    std::vector<Value> a;
    std::vector<std::pair<std::string, Value>> o;   // 有序对象，保证输出稳定

    Value() = default;
    Value(const Value&) = default;
    Value(Value&&) noexcept = default;
    Value& operator=(const Value&) = default;
    Value& operator=(Value&&) noexcept = default;

    static Value null() { return Value{}; }
    static Value boolean(bool v) { Value x; x.t = T::Bool; x.b = v; return x; }
    static Value integer(long long v) { Value x; x.t = T::Int; x.i = v; return x; }
    static Value real(double v) { Value x; x.t = T::Real; x.d = v; return x; }
    static Value str(std::string v) { Value x; x.t = T::Str; x.s = std::move(v); return x; }
    static Value array() { Value x; x.t = T::Arr; return x; }
    static Value object() { Value x; x.t = T::Obj; return x; }

    bool isNull() const { return t == T::Null; }
    bool isBool() const { return t == T::Bool; }
    bool isString() const { return t == T::Str; }
    bool isArray() const { return t == T::Arr; }
    bool isObject() const { return t == T::Obj; }

    void set(std::string key, Value v) { o.emplace_back(std::move(key), std::move(v)); }

    const Value* find(std::string_view key) const {
        for (const auto& kv : o)
            if (kv.first == key) return &kv.second;
        return nullptr;
    }
    Value& push(Value v) {
        a.push_back(std::move(v));
        return a.back();
    }

    std::string getString(std::string_view key, std::string def = {}) const {
        const Value* v = find(key);
        return (v && v->t == T::Str) ? v->s : def;
    }
    long long getInt(std::string_view key, long long def = 0) const {
        const Value* v = find(key);
        if (!v) return def;
        if (v->t == T::Int) return v->i;
        if (v->t == T::Real) return static_cast<long long>(std::llround(v->d));
        if (v->t == T::Bool) return v->b ? 1 : 0;
        return def;
    }
    bool getBool(std::string_view key, bool def = false) const {
        const Value* v = find(key);
        return v && v->t == T::Bool ? v->b : def;
    }
};

// ------------------------------ 序列化 -------------------------------------
inline void writeEscaped(std::string& out, std::string_view sv) {
    out += '"';
    for (unsigned char c : sv) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\u%04x", c);
                out += buf;
            } else {
                out += static_cast<char>(c);   // UTF-8 原样输出
            }
        }
    }
    out += '"';
}

inline void dumpInto(const Value& v, std::string& out, bool pretty, int level) {
    auto newline = [&](int lv) {
        if (!pretty) return;
        out += '\n';
        out.append(static_cast<size_t>(lv) * 2, ' ');
    };
    switch (v.t) {
    case Value::T::Null: out += "null"; break;
    case Value::T::Bool: out += v.b ? "true" : "false"; break;
    case Value::T::Int:  out += std::to_string(v.i); break;
    case Value::T::Real: {
        char buf[64];
        std::snprintf(buf, sizeof buf, "%.10g", v.d);
        out += buf;
        break;
    }
    case Value::T::Str: writeEscaped(out, v.s); break;
    case Value::T::Arr:
        if (v.a.empty()) { out += "[]"; break; }
        out += '[';
        for (size_t k = 0; k < v.a.size(); ++k) {
            if (k) out += ',';
            newline(level + 1);
            dumpInto(v.a[k], out, pretty, level + 1);
        }
        newline(level);
        out += ']';
        break;
    case Value::T::Obj:
        if (v.o.empty()) { out += "{}"; break; }
        out += '{';
        for (size_t k = 0; k < v.o.size(); ++k) {
            if (k) out += ',';
            newline(level + 1);
            writeEscaped(out, v.o[k].first);
            out += pretty ? ": " : ":";
            dumpInto(v.o[k].second, out, pretty, level + 1);
        }
        newline(level);
        out += '}';
        break;
    }
}

inline std::string dump(const Value& v, bool pretty = true) {
    std::string out;
    out.reserve(256);
    dumpInto(v, out, pretty, 0);
    return out;
}

// ------------------------------- 解析 --------------------------------------
class Parser {
public:
    explicit Parser(std::string_view src) : s_(src) {}

    bool run(Value& out, std::string& err) {
        skipWs();
        if (!parseValue(out)) {
            err = err_ + " (offset " + std::to_string(p_) + ")";
            return false;
        }
        skipWs();
        if (p_ != s_.size()) {
            err = "trailing characters at offset " + std::to_string(p_);
            return false;
        }
        return true;
    }

private:
    std::string_view s_;
    size_t p_ = 0;
    std::string err_;

    bool fail(const char* msg) {
        if (err_.empty()) err_ = msg;
        return false;
    }
    void skipWs() {
        while (p_ < s_.size()) {
            char c = s_[p_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++p_;
            else break;
        }
    }
    bool eat(char c) {
        if (p_ < s_.size() && s_[p_] == c) { ++p_; return true; }
        return false;
    }

    bool parseValue(Value& out) {
        if (p_ >= s_.size()) return fail("unexpected end of input");
        char c = s_[p_];
        switch (c) {
        case '{': return parseObject(out);
        case '[': return parseArray(out);
        case '"': {
            std::string tmp;
            if (!parseString(tmp)) return false;
            out = Value::str(std::move(tmp));
            return true;
        }
        case 't':
            if (s_.substr(p_, 4) == "true") { p_ += 4; out = Value::boolean(true); return true; }
            return fail("invalid literal");
        case 'f':
            if (s_.substr(p_, 5) == "false") { p_ += 5; out = Value::boolean(false); return true; }
            return fail("invalid literal");
        case 'n':
            if (s_.substr(p_, 4) == "null") { p_ += 4; out = Value::null(); return true; }
            return fail("invalid literal");
        default: return parseNumber(out);
        }
    }

    bool parseObject(Value& out) {
        out = Value::object();
        eat('{');
        skipWs();
        if (eat('}')) return true;
        for (;;) {
            skipWs();
            if (p_ >= s_.size() || s_[p_] != '"') return fail("expected object key");
            std::string key;
            if (!parseString(key)) return false;
            skipWs();
            if (!eat(':')) return fail("expected ':'");
            skipWs();
            Value v;
            if (!parseValue(v)) return false;
            out.o.emplace_back(std::move(key), std::move(v));
            skipWs();
            if (eat(',')) continue;
            if (eat('}')) return true;
            return fail("expected ',' or '}'");
        }
    }

    bool parseArray(Value& out) {
        out = Value::array();
        eat('[');
        skipWs();
        if (eat(']')) return true;
        for (;;) {
            skipWs();
            Value v;
            if (!parseValue(v)) return false;
            out.a.push_back(std::move(v));
            skipWs();
            if (eat(',')) continue;
            if (eat(']')) return true;
            return fail("expected ',' or ']'");
        }
    }

    static void appendUtf8(std::string& s, unsigned cp) {
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

    bool parseHex4(unsigned& out) {
        if (p_ + 4 > s_.size()) return fail("truncated \\u escape");
        unsigned v = 0;
        for (int k = 0; k < 4; ++k) {
            char c = s_[p_ + static_cast<size_t>(k)];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned>(c - 'A' + 10);
            else return fail("bad hex digit in \\u escape");
        }
        p_ += 4;
        out = v;
        return true;
    }

    bool parseString(std::string& out) {
        out.clear();
        if (!eat('"')) return fail("expected '\"'");
        while (p_ < s_.size()) {
            unsigned char c = static_cast<unsigned char>(s_[p_]);
            if (c == '"') { ++p_; return true; }
            if (c == '\\') {
                ++p_;
                if (p_ >= s_.size()) return fail("truncated escape");
                char e = s_[p_++];
                switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    unsigned cp = 0;
                    if (!parseHex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (p_ + 1 < s_.size() && s_[p_] == '\\' && s_[p_ + 1] == 'u') {
                            p_ += 2;
                            unsigned lo = 0;
                            if (!parseHex4(lo)) return false;
                            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            } else {
                                appendUtf8(out, cp);
                                cp = lo;
                            }
                        }
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default: return fail("unknown escape");
                }
                continue;
            }
            if (c < 0x20) return fail("raw control character in string");
            out += static_cast<char>(c);
            ++p_;
        }
        return fail("unterminated string");
    }

    bool parseNumber(Value& out) {
        size_t start = p_;
        bool real = false;
        if (p_ < s_.size() && (s_[p_] == '-' || s_[p_] == '+')) ++p_;
        bool any = false;
        while (p_ < s_.size()) {
            char c = s_[p_];
            if (c >= '0' && c <= '9') { any = true; ++p_; }
            else if (c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
                if (c == '.' || c == 'e' || c == 'E') real = true;
                ++p_;
            } else break;
        }
        if (!any) return fail("invalid number");
        std::string tok(s_.substr(start, p_ - start));
        if (!real) {
            // 尝试整数，失败时回退到 double
            long long iv = 0;
            if (sscanf_s(tok.c_str(), "%lld", &iv) == 1) {
                out = Value::integer(iv);
                return true;
            }
        }
        out = Value::real(std::strtod(tok.c_str(), nullptr));
        return true;
    }
};

inline bool parse(std::string_view text, Value& out, std::string& err) {
    Parser ps(text);
    return ps.run(out, err);
}

} // namespace jx
