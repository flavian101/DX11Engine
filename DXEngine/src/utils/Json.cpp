#include "dxpch.h"
#include "Json.h"
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace DXEngine
{
    namespace Json
    {
        // ------------------------------------------------------------------ Value
        static const Value kNull;

        int Value::AsInt(int fallback) const
        {
            if (!IsNumber() || !std::isfinite(m_Number)) return fallback;
            const double r = std::round(m_Number);
            return (r >= -2147483648.0 && r <= 2147483647.0) ? static_cast<int>(r) : fallback;
        }

        const Value& Value::At(size_t index) const
        {
            return (IsArray() && index < m_Items.size()) ? m_Items[index] : kNull;
        }

        Value& Value::Push(Value v)
        {
            if (IsNull()) m_Type = Type::Array;
            if (IsArray()) m_Items.push_back(std::move(v));      // misuse (a scalar or an object): nothing happens
            return *this;
        }

        bool Value::Has(const std::string& key) const
        {
            if (!IsObject()) return false;
            for (const std::string& k : m_Keys) if (k == key) return true;
            return false;
        }

        const Value& Value::operator[](const std::string& key) const
        {
            if (IsObject())
                for (size_t i = 0; i < m_Keys.size(); ++i)
                    if (m_Keys[i] == key) return m_Items[i];
            return kNull;
        }

        Value& Value::Set(const std::string& key, Value v)
        {
            if (IsNull()) m_Type = Type::Object;
            if (!IsObject()) return *this;                       // misuse (a scalar or an array): nothing happens
            for (size_t i = 0; i < m_Keys.size(); ++i)
                if (m_Keys[i] == key) { m_Items[i] = std::move(v); return *this; }
            m_Keys.push_back(key);
            m_Items.push_back(std::move(v));
            return *this;
        }

        // ------------------------------------------------------------------ writing
        struct Writer
        {
            static void Escape(const std::string& s, std::string& out)
            {
                out += '"';
                for (unsigned char c : s)
                {
                    switch (c)
                    {
                    case '"':  out += "\\\""; break;
                    case '\\': out += "\\\\"; break;
                    case '\n': out += "\\n"; break;
                    case '\r': out += "\\r"; break;
                    case '\t': out += "\\t"; break;
                    case '\b': out += "\\b"; break;
                    case '\f': out += "\\f"; break;
                    default:
                        if (c < 0x20) { char buf[8]; std::snprintf(buf, sizeof(buf), "\\u%04x", c); out += buf; }
                        else out += static_cast<char>(c);          // UTF-8 passes through
                    }
                }
                out += '"';
            }

            static void Number(const Value& v, std::string& out)
            {
                if (!std::isfinite(v.m_Number)) { out += "null"; return; }          // JSON has no NaN / Infinity
                char buf[64];
                std::to_chars_result r;
                if (v.m_IsFloat) r = std::to_chars(buf, buf + sizeof(buf), static_cast<float>(v.m_Number));
                else             r = std::to_chars(buf, buf + sizeof(buf), v.m_Number);
                out.append(buf, r.ptr);
            }

            static bool Flat(const Value& v)       // an array of a few scalars: one line
            {
                if (!v.IsArray() || v.m_Items.size() > 16) return false;
                for (const Value& e : v.m_Items) if (e.IsArray() || e.IsObject()) return false;
                return true;
            }

            static void Write(const Value& v, int indent, int level, std::string& out)
            {
                const auto pad = [&](int l) { out.append(static_cast<size_t>(indent * l), ' '); };
                switch (v.m_Type)
                {
                case Value::Type::Null:   out += "null"; break;
                case Value::Type::Bool:   out += v.m_Bool ? "true" : "false"; break;
                case Value::Type::Number: Number(v, out); break;
                case Value::Type::String: Escape(v.m_String, out); break;
                case Value::Type::Array:
                    if (v.m_Items.empty()) { out += "[]"; break; }
                    if (Flat(v) || indent <= 0)
                    {
                        out += '[';
                        for (size_t i = 0; i < v.m_Items.size(); ++i) { if (i) out += indent > 0 ? ", " : ","; Write(v.m_Items[i], indent, level + 1, out); }
                        out += ']';
                        break;
                    }
                    out += "[\n";
                    for (size_t i = 0; i < v.m_Items.size(); ++i)
                    {
                        pad(level + 1); Write(v.m_Items[i], indent, level + 1, out);
                        out += (i + 1 < v.m_Items.size()) ? ",\n" : "\n";
                    }
                    pad(level); out += ']';
                    break;
                case Value::Type::Object:
                    if (v.m_Keys.empty()) { out += "{}"; break; }
                    out += indent > 0 ? "{\n" : "{";
                    for (size_t i = 0; i < v.m_Keys.size(); ++i)
                    {
                        if (indent > 0) pad(level + 1);
                        Escape(v.m_Keys[i], out); out += indent > 0 ? ": " : ":";
                        Write(v.m_Items[i], indent, level + 1, out);
                        if (i + 1 < v.m_Keys.size()) out += indent > 0 ? ",\n" : ",";
                        else if (indent > 0) out += '\n';
                    }
                    if (indent > 0) pad(level);
                    out += '}';
                    break;
                }
            }
        };

        std::string Value::Dump(int indent) const
        {
            std::string out;
            Writer::Write(*this, indent, 0, out);
            if (indent > 0) out += '\n';
            return out;
        }

        // ------------------------------------------------------------------ parsing
        namespace
        {
            class Parser
            {
            public:
                explicit Parser(const std::string& t) : m_Text(t) { if (t.compare(0, 3, "\xEF\xBB\xBF") == 0) m_Pos = 3; }

                ParseResult Run()
                {
                    ParseResult r;
                    SkipSpace();
                    if (!ParseValue(r.value, 0)) { r.error = m_Error; r.value = Value(); return r; }
                    SkipSpace();
                    if (m_Pos != m_Text.size()) { Fail("unexpected text after the value"); r.error = m_Error; r.value = Value(); return r; }
                    r.ok = true;
                    return r;
                }

            private:
                const std::string& m_Text;
                size_t m_Pos = 0;
                std::string m_Error;

                bool Fail(const std::string& what)
                {
                    if (m_Error.empty())
                    {
                        int line = 1, col = 1;
                        for (size_t i = 0; i < m_Pos && i < m_Text.size(); ++i) { if (m_Text[i] == '\n') { ++line; col = 1; } else ++col; }
                        m_Error = std::to_string(line) + ":" + std::to_string(col) + ": " + what;
                    }
                    return false;
                }

                bool End() const { return m_Pos >= m_Text.size(); }
                void SkipSpace() { while (!End() && (m_Text[m_Pos] == ' ' || m_Text[m_Pos] == '\t' || m_Text[m_Pos] == '\n' || m_Text[m_Pos] == '\r')) ++m_Pos; }

                bool Literal(const char* word)
                {
                    const size_t n = std::char_traits<char>::length(word);
                    if (m_Text.compare(m_Pos, n, word) != 0) return false;
                    m_Pos += n;
                    return true;
                }

                static void AppendUtf8(std::string& s, unsigned cp)
                {
                    if (cp < 0x80) s += static_cast<char>(cp);
                    else if (cp < 0x800) { s += static_cast<char>(0xC0 | (cp >> 6)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
                    else if (cp < 0x10000) { s += static_cast<char>(0xE0 | (cp >> 12)); s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
                    else { s += static_cast<char>(0xF0 | (cp >> 18)); s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
                }

                bool Hex4(unsigned& out)
                {
                    if (m_Pos + 4 > m_Text.size()) return Fail("a \\u escape needs four hex digits");
                    unsigned v = 0;
                    for (int i = 0; i < 4; ++i)
                    {
                        const char c = m_Text[m_Pos + static_cast<size_t>(i)];
                        v <<= 4;
                        if (c >= '0' && c <= '9') v |= static_cast<unsigned>(c - '0');
                        else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned>(c - 'a' + 10);
                        else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned>(c - 'A' + 10);
                        else return Fail("a \\u escape needs four hex digits");
                    }
                    m_Pos += 4;
                    out = v;
                    return true;
                }

                bool ParseString(std::string& out)
                {
                    ++m_Pos;                                          // the opening quote
                    out.clear();
                    while (!End())
                    {
                        const unsigned char c = static_cast<unsigned char>(m_Text[m_Pos]);
                        if (c == '"') { ++m_Pos; return true; }
                        if (c < 0x20) return Fail("a control character inside a string");
                        if (c != '\\') { out += static_cast<char>(c); ++m_Pos; continue; }
                        ++m_Pos;
                        if (End()) break;
                        const char e = m_Text[m_Pos++];
                        switch (e)
                        {
                        case '"': out += '"'; break;   case '\\': out += '\\'; break;  case '/': out += '/'; break;
                        case 'b': out += '\b'; break;  case 'f': out += '\f'; break;   case 'n': out += '\n'; break;
                        case 'r': out += '\r'; break;  case 't': out += '\t'; break;
                        case 'u':
                        {
                            unsigned cp;
                            if (!Hex4(cp)) return false;
                            if (cp >= 0xD800 && cp <= 0xDBFF)          // a high surrogate: the low one must follow
                            {
                                unsigned lo;
                                if (m_Text.compare(m_Pos, 2, "\\u") != 0) return Fail("a high surrogate without its low half");
                                m_Pos += 2;
                                if (!Hex4(lo)) return false;
                                if (lo < 0xDC00 || lo > 0xDFFF) return Fail("a high surrogate without its low half");
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            }
                            else if (cp >= 0xDC00 && cp <= 0xDFFF) return Fail("a lone low surrogate");
                            AppendUtf8(out, cp);
                            break;
                        }
                        default: --m_Pos; return Fail(std::string("unknown escape \\") + e);
                        }
                    }
                    return Fail("a string that never ends");
                }

                bool ParseNumber(Value& out)
                {
                    const size_t start = m_Pos;
                    if (m_Text[m_Pos] == '-') ++m_Pos;
                    const auto digits = [&] { size_t n = 0; while (!End() && m_Text[m_Pos] >= '0' && m_Text[m_Pos] <= '9') { ++m_Pos; ++n; } return n; };
                    if (End() || m_Text[m_Pos] < '0' || m_Text[m_Pos] > '9') return Fail("a number needs digits");
                    if (m_Text[m_Pos] == '0') ++m_Pos; else digits();                    // no leading zeros (JSON)
                    if (!End() && m_Text[m_Pos] == '.') { ++m_Pos; if (digits() == 0) return Fail("digits are needed after the decimal point"); }
                    if (!End() && (m_Text[m_Pos] == 'e' || m_Text[m_Pos] == 'E'))
                    {
                        ++m_Pos;
                        if (!End() && (m_Text[m_Pos] == '+' || m_Text[m_Pos] == '-')) ++m_Pos;
                        if (digits() == 0) return Fail("digits are needed in the exponent");
                    }
                    const std::string token = m_Text.substr(start, m_Pos - start);
                    out = Value(std::strtod(token.c_str(), nullptr));
                    return true;
                }

                bool ParseValue(Value& out, int depth)
                {
                    if (depth > kMaxDepth) return Fail("nested too deeply");
                    SkipSpace();
                    if (End()) return Fail("the value is missing");
                    const char c = m_Text[m_Pos];
                    if (c == '{') return ParseObject(out, depth);
                    if (c == '[') return ParseArray(out, depth);
                    if (c == '"') { std::string s; if (!ParseString(s)) return false; out = Value(std::move(s)); return true; }
                    if (c == '-' || (c >= '0' && c <= '9')) return ParseNumber(out);
                    if (Literal("true"))  { out = Value(true);  return true; }
                    if (Literal("false")) { out = Value(false); return true; }
                    if (Literal("null"))  { out = Value();      return true; }
                    return Fail(std::string("unexpected '") + c + "'");
                }

                bool ParseArray(Value& out, int depth)
                {
                    ++m_Pos;
                    out = Value::MakeArray();
                    SkipSpace();
                    if (!End() && m_Text[m_Pos] == ']') { ++m_Pos; return true; }
                    for (;;)
                    {
                        Value element;
                        if (!ParseValue(element, depth + 1)) return false;
                        out.Push(std::move(element));
                        SkipSpace();
                        if (End()) return Fail("an array that never ends");
                        if (m_Text[m_Pos] == ',') { ++m_Pos; continue; }
                        if (m_Text[m_Pos] == ']') { ++m_Pos; return true; }
                        return Fail("expected ',' or ']'");
                    }
                }

                bool ParseObject(Value& out, int depth)
                {
                    ++m_Pos;
                    out = Value::MakeObject();
                    SkipSpace();
                    if (!End() && m_Text[m_Pos] == '}') { ++m_Pos; return true; }
                    for (;;)
                    {
                        SkipSpace();
                        if (End() || m_Text[m_Pos] != '"') return Fail("expected a key (a string)");
                        std::string key;
                        if (!ParseString(key)) return false;
                        SkipSpace();
                        if (End() || m_Text[m_Pos] != ':') return Fail("expected ':' after the key");
                        ++m_Pos;
                        Value member;
                        if (!ParseValue(member, depth + 1)) return false;
                        out.Set(key, std::move(member));                  // a repeated key: the last one wins
                        SkipSpace();
                        if (End()) return Fail("an object that never ends");
                        if (m_Text[m_Pos] == ',') { ++m_Pos; continue; }
                        if (m_Text[m_Pos] == '}') { ++m_Pos; return true; }
                        return Fail("expected ',' or '}'");
                    }
                }
            };
        }

        ParseResult Parse(const std::string& text)
        {
            return Parser(text).Run();
        }
    }
}
