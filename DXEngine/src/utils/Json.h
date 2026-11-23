#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace DXEngine
{
    // ==================================================================
    // Json: a small JSON document model, parser and writer, for the files the engine saves (scenes first). No dependency.
    //
    //   * Objects keep the order the keys were added in (a saved file stays diffable), keys are unique (Set replaces).
    //   * Reading never throws and never asserts: a missing key, a wrong type or an out-of-range index gives a default (see
    //     the Get / Is helpers), and Parse reports a malformed document as "line:column: what" instead of crashing.
    //   * Numbers are doubles. A float stored with Value(float) is written with the SHORTEST text that reads back as the very
    //     same float (0.1f is "0.1", not "0.10000000149011612"), so a save / load never changes a value. 64-bit ids do not fit
    //     a double: store them as strings (Uuid::ToString).
    //   * Nesting is limited to kMaxDepth levels: a hostile or corrupt file cannot overflow the stack.
    // ==================================================================
    namespace Json
    {
        constexpr int kMaxDepth = 128;

        class Value
        {
        public:
            enum class Type { Null, Bool, Number, String, Array, Object };

            Value() = default;                                   // null
            Value(bool b) : m_Type(Type::Bool), m_Bool(b) {}
            Value(int n) : m_Type(Type::Number), m_Number(n) {}
            Value(unsigned n) : m_Type(Type::Number), m_Number(n) {}
            Value(double n) : m_Type(Type::Number), m_Number(n) {}
            Value(float n) : m_Type(Type::Number), m_Number(n), m_IsFloat(true) {}
            Value(const char* s) : m_Type(Type::String), m_String(s ? s : "") {}
            Value(std::string s) : m_Type(Type::String), m_String(std::move(s)) {}

            static Value MakeArray()  { Value v; v.m_Type = Type::Array;  return v; }
            static Value MakeObject() { Value v; v.m_Type = Type::Object; return v; }

            Type GetType() const { return m_Type; }
            bool IsNull() const   { return m_Type == Type::Null; }
            bool IsBool() const   { return m_Type == Type::Bool; }
            bool IsNumber() const { return m_Type == Type::Number; }
            bool IsString() const { return m_Type == Type::String; }
            bool IsArray() const  { return m_Type == Type::Array; }
            bool IsObject() const { return m_Type == Type::Object; }

            // Reading: the fallback when the value is not of that type.
            bool               AsBool(bool fallback = false) const { return IsBool() ? m_Bool : fallback; }
            double             AsNumber(double fallback = 0.0) const { return IsNumber() ? m_Number : fallback; }
            float              AsFloat(float fallback = 0.0f) const { return IsNumber() ? static_cast<float>(m_Number) : fallback; }
            int                AsInt(int fallback = 0) const;                      // rounds; out of int range = fallback
            const std::string& AsString() const { static const std::string kEmpty; return IsString() ? m_String : kEmpty; }

            // Arrays (an object or scalar has Size() 0)
            size_t       Size() const { return IsArray() ? m_Items.size() : (IsObject() ? m_Items.size() : 0); }
            const Value& At(size_t index) const;                                   // null Value when out of range / not an array
            Value&       Push(Value v);                                            // appends; makes this an array first if it is null. Returns *this: chain them

            // Objects
            bool         Has(const std::string& key) const;
            const Value& operator[](const std::string& key) const;                 // null Value when missing / not an object
            Value&       Set(const std::string& key, Value v);                     // adds or replaces; makes this an object first if it is null. Returns *this: chain them
            const std::vector<std::string>& Keys() const { return m_Keys; }        // in insertion order (empty for non-objects)

            // Text. indent = spaces per level; arrays of at most 16 scalars stay on one line.
            std::string Dump(int indent = 2) const;

        private:
            friend struct Writer;
            Type   m_Type = Type::Null;
            bool   m_Bool = false;
            double m_Number = 0.0;
            bool   m_IsFloat = false;                   // write the shortest text that reads back as the same FLOAT
            std::string m_String;
            std::vector<std::string> m_Keys;            // objects: the keys, parallel to m_Items
            std::vector<Value>       m_Items;           // arrays: the elements; objects: the values
        };

        struct ParseResult
        {
            bool        ok = false;
            Value       value;
            std::string error;                          // "line:column: what", empty when ok
        };

        // The whole text must be exactly one JSON value (plus whitespace). A UTF-8 byte order mark is skipped.
        ParseResult Parse(const std::string& text);
    }
}
