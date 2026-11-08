// RHI rules that need no GPU: the checks every backend shares before it creates a buffer.
// (The D3D11 code that applies them needs a device and is exercised by running the Sandbox.)
#include "RHI/GraphicsTypes.h"
#include <cstdio>

using namespace DXEngine::RHI;

static int g_failed = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("  FAILED line %d: %s\n", __LINE__, #cond); ++g_failed; } } while (0)

static BufferDesc LightBuffer()   // what the structured light buffer asks for
{
    BufferDesc d;
    d.type = BufferType::Storage;
    d.usage = BufferUsage::Dynamic;
    d.stride = 64;
    d.size = 64 * 256;
    d.debugName = "Scene.LocalLights";
    return d;
}

static void TestDynamicStructuredBufferIsValid()
{
    CHECK(ValidateBufferDesc(LightBuffer()).empty());
}

static void TestStructuredBufferNeedsAGoodStride()
{
    BufferDesc d = LightBuffer();
    d.stride = 0;                       CHECK(!ValidateBufferDesc(d).empty());
    d.stride = 6;  d.size = 6 * 10;     CHECK(!ValidateBufferDesc(d).empty());   // not a multiple of 4
    d.stride = kMaxStructuredStride + 4; d.size = d.stride; CHECK(!ValidateBufferDesc(d).empty());
    d.stride = kMaxStructuredStride; d.size = d.stride;     CHECK(ValidateBufferDesc(d).empty());   // the limit itself is fine
    d.stride = 64; d.size = 64 * 10 + 4;                    CHECK(!ValidateBufferDesc(d).empty());   // not a whole number of elements
}

static void TestUnorderedAccessIsForStorageOnly()
{
    BufferDesc d = LightBuffer();
    d.allowUnorderedAccess = true;
    CHECK(ValidateBufferDesc(d).empty());

    BufferDesc v;
    v.type = BufferType::Vertex; v.usage = BufferUsage::Dynamic; v.size = 128;
    v.allowUnorderedAccess = true;
    CHECK(!ValidateBufferDesc(v).empty());
}

static void TestStaticBuffersNeedData()
{
    const float data[4] = { 0, 1, 2, 3 };
    BufferDesc v;
    v.type = BufferType::Vertex; v.usage = BufferUsage::Static; v.size = sizeof(data);
    CHECK(!ValidateBufferDesc(v).empty());
    v.initialData = data;
    CHECK(ValidateBufferDesc(v).empty());

    BufferDesc gpu = LightBuffer();      // a GPU-written structured buffer is never "static"
    gpu.usage = BufferUsage::Static; gpu.allowUnorderedAccess = true;
    CHECK(ValidateBufferDesc(gpu).empty());
}

static void TestZeroSizeAndNaming()
{
    BufferDesc d = LightBuffer();
    d.size = 0;
    const std::string msg = ValidateBufferDesc(d);
    CHECK(!msg.empty());
    CHECK(msg.find("Scene.LocalLights") != std::string::npos);    // the message names the buffer
}

int main()
{
    struct { const char* name; void (*fn)(); } tests[] = {
        { "buffers: dynamic structured buffer is valid",   TestDynamicStructuredBufferIsValid },
        { "buffers: structured buffer stride rules",       TestStructuredBufferNeedsAGoodStride },
        { "buffers: unordered access is Storage-only",     TestUnorderedAccessIsForStorageOnly },
        { "buffers: static buffers need initial data",     TestStaticBuffersNeedData },
        { "buffers: zero size, message names the buffer",  TestZeroSizeAndNaming },
    };
    for (auto& t : tests)
    {
        const int before = g_failed;
        t.fn();
        std::printf("%s  %s\n", g_failed == before ? "[ ok ]" : "[FAIL]", t.name);
    }
    std::printf("\n%s\n", g_failed == 0 ? "all rhi tests passed" : "SOME RHI TESTS FAILED");
    return g_failed == 0 ? 0 : 1;
}
