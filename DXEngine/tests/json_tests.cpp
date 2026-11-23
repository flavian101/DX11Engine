// Json: the document model, the writer and the parser, without a GPU or a file.
#include "utils/Json.h"
#include <cmath>
#include <cstdio>
#include <string>

using namespace DXEngine;

static int g_failed = 0;
#define CHECK(c) do { if (!(c)) { std::printf("  FAILED line %d: %s\n", __LINE__, #c); ++g_failed; } } while (0)

static void TestScalarsAndDefaults()
{
    Json::Value v = Json::Value::MakeObject();
    v.Set("b", true).Set("n", 42).Set("s", "text");
    CHECK(v["b"].AsBool() && v["n"].AsInt() == 42 && v["s"].AsString() == "text");
    CHECK(v["missing"].IsNull());                                   // a missing key is null, not an error
    CHECK(v["n"].AsString().empty() && !v["s"].AsBool() && v["s"].AsNumber(7.0) == 7.0);   // wrong type = the fallback
    CHECK(v["missing"]["deeper"]["still"].IsNull());                // chains of lookups on nothing stay safe
    CHECK(v.At(0).IsNull() && v.Size() == 3);
    CHECK(Json::Value(1e300).AsInt(5) == 5);                         // out of range: the fallback, no overflow
    CHECK(Json::Value(2.5).AsInt() == 3);                            // rounds
}

static void TestChaining()
{
    Json::Value v;
    v.Set("a", 1).Set("b", 2).Set("c", Json::Value::MakeArray().Push(1).Push(2).Push(3));
    CHECK(v.Size() == 3 && v["b"].AsInt() == 2 && v["c"].Size() == 3 && v["c"].At(2).AsInt() == 3);
    CHECK(v.Dump(0) == "{\"a\":1,\"b\":2,\"c\":[1,2,3]}");
}

static void TestObjectOrderAndReplace()
{
    Json::Value v;
    v.Set("zeta", 1); v.Set("alpha", 2); v.Set("mid", 3);
    CHECK(v.Keys().size() == 3 && v.Keys()[0] == "zeta" && v.Keys()[1] == "alpha" && v.Keys()[2] == "mid");   // insertion order, not sorted
    v.Set("alpha", 20);                                              // replaces in place
    CHECK(v.Keys().size() == 3 && v["alpha"].AsInt() == 20 && v.Keys()[1] == "alpha");
    CHECK(v.Dump(0) == "{\"zeta\":1,\"alpha\":20,\"mid\":3}");
}

static void TestFloatsRoundTripExactly()
{
    const float values[] = { 0.1f, 1.0f / 3.0f, 3.14159274f, 1e-7f, 123456.789f, -0.0f, 16777216.0f, 3.4028235e38f };
    for (float f : values)
    {
        Json::Value v = Json::Value::MakeArray();
        v.Push(f);
        const Json::ParseResult back = Json::Parse(v.Dump(0));
        CHECK(back.ok);
        CHECK(back.value.At(0).AsFloat() == f);                      // bit for bit the same float
    }
    Json::Value one = Json::Value::MakeArray();
    one.Push(0.1f);
    CHECK(one.Dump(0) == "[0.1]");                                   // the SHORT text, not 0.10000000149011612
    Json::Value dbl = Json::Value::MakeArray();
    dbl.Push(0.1);
    CHECK(dbl.Dump(0) == "[0.1]");
    Json::Value integral = Json::Value::MakeArray();
    integral.Push(3); integral.Push(2.0);
    CHECK(integral.Dump(0) == "[3,2]");                              // integral values have no decimal point
}

static void TestNonFiniteBecomesNull()
{
    Json::Value v = Json::Value::MakeArray();
    v.Push(std::nanf("")); v.Push(INFINITY);
    CHECK(v.Dump(0) == "[null,null]");                               // JSON has no NaN; the file stays valid
    CHECK(Json::Parse(v.Dump(0)).ok);
}

static void TestRoundTripOfADocument()
{
    Json::Value doc = Json::Value::MakeObject();
    doc.Set("name", "Cube \"one\"\n\twith\\escapes \xC3\xA9 \xE2\x82\xAC");
    Json::Value list = Json::Value::MakeArray();
    for (int i = 0; i < 40; ++i) { Json::Value o = Json::Value::MakeObject(); o.Set("i", i).Set("v", Json::Value::MakeArray().Push(i).Push("x")); list.Push(o); }
    doc.Set("list", list);
    doc.Set("empty", Json::Value::MakeObject());
    doc.Set("nothing", Json::Value());
    for (int indent : { 0, 2, 4 })
    {
        const Json::ParseResult r = Json::Parse(doc.Dump(indent));
        CHECK(r.ok);
        CHECK(r.value["name"].AsString() == doc["name"].AsString());
        CHECK(r.value["list"].Size() == 40 && r.value["list"].At(39)["i"].AsInt() == 39);
        CHECK(r.value["list"].At(7)["v"].Size() == 2 && r.value["list"].At(7)["v"].At(0).AsInt() == 7);   // nested members survived
        CHECK(r.value["empty"].IsObject() && r.value["nothing"].IsNull() && r.value.Has("nothing"));
        CHECK(r.value.Dump(2) == doc.Dump(2));                       // reading what was written writes the same text
    }
}

static void TestFlatArraysStayOnOneLine()
{
    Json::Value v = Json::Value::MakeObject();
    Json::Value pos = Json::Value::MakeArray();
    pos.Push(1.5f); pos.Push(2.0f); pos.Push(-3.0f);
    v.Set("position", pos);
    CHECK(v.Dump(2) == "{\n  \"position\": [1.5, 2, -3]\n}\n");
}

static void TestParseAccepts()
{
    CHECK(Json::Parse("  { \"a\" : [ 1 , 2.5e2 , -0.5E-1 , true , false , null , \"x\" ] }  ").ok);
    CHECK(Json::Parse("[]").ok && Json::Parse("{}").ok && Json::Parse("0").ok && Json::Parse("\"\"").ok);
    CHECK(Json::Parse("\xEF\xBB\xBF{\"bom\":1}").ok);                // a byte order mark (Windows editors add one)
    const auto r = Json::Parse("\"\\u0041\\u00e9\\u20ac\\ud83d\\ude00\\n\\/\"");
    CHECK(r.ok && r.value.AsString() == "A\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80\n/");   // escapes, a BMP char, a surrogate pair
    CHECK(Json::Parse("{\"k\":1,\"k\":2}").value["k"].AsInt() == 2); // a repeated key: the last one wins
    CHECK(Json::Parse("-12.5e1").value.AsNumber() == -125.0);
}

static void TestParseRejectsWithPosition()
{
    const char* bad[] = {
        "", "   ", "{", "[1,", "[1 2]", "{\"a\"}", "{\"a\":}", "{a:1}", "{\"a\":1,}", "[1,]", "tru", "nul", "01", "1.", ".5", "1e", "+1",
        "\"abc", "\"a\nb\"", "\"\\x\"", "\"\\u12\"", "\"\\ud83d\"", "\"\\ude00\"", "[1] 2", "{} {}", "'a'", "[\"a\" \"b\"]"
    };
    for (const char* text : bad)
    {
        const Json::ParseResult r = Json::Parse(text);
        if (r.ok) std::printf("  accepted: %s\n", text);
        CHECK(!r.ok);
        CHECK(!r.error.empty() && r.error.find(':') != std::string::npos);
        CHECK(r.value.IsNull());                                     // nothing half-built is handed out
    }
    const Json::ParseResult where = Json::Parse("{\n  \"a\": 1,\n  \"b\": ?\n}");
    CHECK(!where.ok && where.error.rfind("3:", 0) == 0);             // reported on line 3
}

static void TestDepthIsLimited()
{
    std::string deep(5000, '[');
    CHECK(!Json::Parse(deep).ok);                                    // an error, not a stack overflow
    std::string ok;
    for (int i = 0; i < Json::kMaxDepth; ++i) ok += '[';
    for (int i = 0; i < Json::kMaxDepth; ++i) ok += ']';
    CHECK(Json::Parse(ok).ok);                                       // the limit itself is allowed
    std::string tooDeep;
    for (int i = 0; i < Json::kMaxDepth + 2; ++i) tooDeep += '[';
    for (int i = 0; i < Json::kMaxDepth + 2; ++i) tooDeep += ']';
    CHECK(!Json::Parse(tooDeep).ok);
    std::string objects;
    for (int i = 0; i < 3000; ++i) objects += "{\"a\":";
    CHECK(!Json::Parse(objects).ok);
}

static void TestMisuseIsHarmless()
{
    Json::Value scalar(5);
    scalar.Push(1);                                                  // not an array: nothing happens
    scalar.Set("k", 1);                                              // not an object: nothing happens
    CHECK(scalar.IsNumber() && scalar.AsInt() == 5);
    Json::Value arr = Json::Value::MakeArray();
    arr.Set("k", 1);
    CHECK(arr.Size() == 0 && arr.IsArray());
}

int main()
{
    struct Test { const char* name; void (*fn)(); };
    const Test tests[] = {
        { "scalars, defaults, safe lookups",       TestScalarsAndDefaults },
        { "Set / Push chain",                      TestChaining },
        { "objects keep insertion order",          TestObjectOrderAndReplace },
        { "floats round trip exactly",             TestFloatsRoundTripExactly },
        { "NaN / Infinity become null",            TestNonFiniteBecomesNull },
        { "a document round trips",                TestRoundTripOfADocument },
        { "short numeric arrays stay on a line",   TestFlatArraysStayOnOneLine },
        { "parse accepts valid JSON",              TestParseAccepts },
        { "parse rejects bad JSON with a position", TestParseRejectsWithPosition },
        { "nesting depth is limited",              TestDepthIsLimited },
        { "misuse is harmless",                    TestMisuseIsHarmless },
    };
    for (const Test& t : tests)
    {
        const int before = g_failed;
        t.fn();
        std::printf("%s  %s\n", g_failed == before ? "PASS" : "FAIL", t.name);
    }
    std::printf(g_failed ? "\n%d check(s) FAILED\n" : "\nAll json tests passed\n", g_failed);
    return g_failed ? 1 : 0;
}
