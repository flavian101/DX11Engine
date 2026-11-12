// Materials without a GPU. The texture slot table (MaterialSlots) is the one description of a material texture slot,
// and these tests are what keeps it honest: ordering, uniqueness, registers, the generated #defines and HLSL
// declarations, and that a Material's texture mask can never disagree with the textures it holds. Also the colour-space
// aware mip generation (ImageMips): sRGB images are filtered in linear light, data images as plain numbers.
#include "utils/ConstantBufferTypes.h"
#include "utils/ImageMips.h"
#include "utils/material/Material.h"
#include "utils/material/MaterialSlots.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace DXEngine;

static int g_failed = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("  FAILED line %d: %s\n", __LINE__, #cond); ++g_failed; } } while (0)

// A texture that is only a handle: enough for a Material to hold.
struct FakeTexture : RHI::ITexture
{
    std::string name = "fake";
    const std::string& GetDebugName() const override { return name; }
    void SetDebugName(const std::string& n) override { name = n; }
    uint64_t GetGPUHandle() const override { return 0; }
    size_t GetMemoryUsage() const override { return 0; }
    bool IsValid() const override { return true; }
    RHI::TextureType GetType() const override { return RHI::TextureType::Texture2D; }
    RHI::TextureFormat GetFormat() const override { return RHI::TextureFormat::RGBA8_UNORM; }
    uint32_t GetWidth() const override { return 1; }
    uint32_t GetHeight() const override { return 1; }
    uint32_t GetDepth() const override { return 1; }
    uint32_t GetMipLevels() const override { return 1; }
    uint32_t GetArraySize() const override { return 1; }
    uint32_t GetSampleCount() const override { return 1; }
    bool Update(const void*, uint32_t, uint32_t) override { return true; }
    bool GenerateMips() override { return true; }
    bool ReadPixels(void*, uint32_t, uint32_t) override { return true; }
    void* GetNativeHandle() const override { return nullptr; }
};
static std::shared_ptr<RHI::ITexture> Tex() { return std::make_shared<FakeTexture>(); }

static bool IsIdentifier(const std::string& s)
{
    if (s.empty() || !(std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_')) return false;
    for (char c : s) if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
    return true;
}

static std::vector<std::string> Lines(const std::string& text)
{
    std::vector<std::string> out; std::istringstream in(text); std::string l;
    while (std::getline(in, l)) out.push_back(l);
    return out;
}

static bool Contains(const std::string& hay, const std::string& needle) { return hay.find(needle) != std::string::npos; }
static int CountOf(const std::string& hay, const std::string& needle)
{
    int n = 0; for (size_t p = hay.find(needle); p != std::string::npos; p = hay.find(needle, p + needle.size())) ++n; return n;
}

// ----------------------------------------------------------------------------------------- the table
static void TestTableShape()
{
    const auto& rows = MaterialSlots::All();
    CHECK(rows.size() == kTextureSlotCount);                          // a slot without a row (or the reverse) fails here
    for (uint32_t i = 0; i < rows.size(); ++i)
    {
        CHECK(static_cast<uint32_t>(rows[i].slot) == i);              // row i IS slot i: register == slot == index
        CHECK(MaterialSlots::IsValid(rows[i].slot));
        CHECK(&MaterialSlots::Info(rows[i].slot) == &rows[i]);
    }
    CHECK(!MaterialSlots::IsValid(TextureSlot::Count));
    CHECK(!MaterialSlots::IsValid(static_cast<TextureSlot>(99)));
    CHECK(std::string(MaterialSlots::Name(static_cast<TextureSlot>(99))) == "?");
    CHECK(kTextureSlotCount <= kMaterialRegisterLimit);
}

static void TestTableNamesAreUniqueAndValid()
{
    std::set<std::string> names, hlsl, defines;
    for (const SlotInfo& r : MaterialSlots::All())
    {
        CHECK(names.insert(r.name).second);                           // two rows with one name would make logs ambiguous
        CHECK(hlsl.insert(r.hlslName).second);                        // ... two with one HLSL variable would not compile
        CHECK(defines.insert(r.define).second);                       // ... two with one define would switch both on together
        CHECK(IsIdentifier(r.name) && IsIdentifier(r.hlslName) && IsIdentifier(r.define));
        CHECK(std::string(r.define).rfind("HAS_", 0) == 0);
        const std::string type = r.hlslType;
        CHECK(type == "Texture2D" || type == "TextureCube");
        CHECK((type == "TextureCube") == r.isCube);
    }
    // the derived switches must not collide with a slot define
    CHECK(defines.count("HAS_DETAIL_TEXTURES") == 0 && defines.count("ENABLE_PARALLAX_MAPPING") == 0);
}

static void TestEngineRegistersStayClear()
{
    // material textures are t0..t(Count-1); everything the engine binds itself must start above t31
    CHECK(SRV_Lights >= kMaterialRegisterLimit);
    CHECK(SRV_ShadowAtlas >= kMaterialRegisterLimit && SRV_ShadowViews >= kMaterialRegisterLimit);
    CHECK(SRV_ClusterRanges >= kMaterialRegisterLimit && SRV_LightIndices >= kMaterialRegisterLimit);
}

static void TestColourSpaceRule()
{
    // The rule: COLOURS are sRGB (albedo, emissive, detail albedo, the sky), DATA is linear (everything else).
    // Reading data as colour (or the other way round) silently changes the numbers, so the set is pinned here.
    const std::set<TextureSlot> colour = { TextureSlot::Diffuse, TextureSlot::Emissive, TextureSlot::DetailDiffuse,
                                           TextureSlot::Environment, TextureSlot::Irradiance };
    for (const SlotInfo& r : MaterialSlots::All())
        CHECK((r.colorSpace == TextureColorSpace::SRGB) == (colour.count(r.slot) == 1));
}

// ----------------------------------------------------------------------------------------- generated text
static void TestMask()
{
    CHECK(MaterialSlots::MaskBit(TextureSlot::Diffuse) == 1u);
    CHECK(MaterialSlots::MaskBit(TextureSlot::Normal) == 2u);
    CHECK(MaterialSlots::MaskBit(TextureSlot::BRDF_LUT) == (1u << 13));
    std::set<uint32_t> bits;
    for (const SlotInfo& r : MaterialSlots::All()) CHECK(bits.insert(MaterialSlots::MaskBit(r.slot)).second);   // one bit each
    const uint32_t m = MaterialSlots::MaskBit(TextureSlot::Normal) | MaterialSlots::MaskBit(TextureSlot::Height);
    CHECK(MaterialSlots::Has(m, TextureSlot::Normal) && MaterialSlots::Has(m, TextureSlot::Height) && !MaterialSlots::Has(m, TextureSlot::Diffuse));
}

static void TestGeneratedDefines()
{
    CHECK(MaterialSlots::GenerateTextureDefines(0).empty());
    CHECK(MaterialSlots::GenerateTextureDefines(MaterialSlots::MaskBit(TextureSlot::Diffuse)) == "#define HAS_DIFFUSE_TEXTURE 1\n");

    // slot order, whatever order the bits were set in
    const std::string two = MaterialSlots::GenerateTextureDefines(MaterialSlots::MaskBit(TextureSlot::Normal) | MaterialSlots::MaskBit(TextureSlot::Diffuse));
    CHECK(two == "#define HAS_DIFFUSE_TEXTURE 1\n#define HAS_NORMAL_MAP 1\n");

    // derived switches
    const std::string height = MaterialSlots::GenerateTextureDefines(MaterialSlots::MaskBit(TextureSlot::Height));
    CHECK(Contains(height, "#define HAS_HEIGHT_MAP 1\n") && Contains(height, "#define ENABLE_PARALLAX_MAPPING 1\n"));
    const std::string dd = MaterialSlots::GenerateTextureDefines(MaterialSlots::MaskBit(TextureSlot::DetailDiffuse));
    CHECK(Contains(dd, "HAS_DETAIL_DIFFUSE_MAP") && CountOf(dd, "HAS_DETAIL_TEXTURES") == 1);
    const std::string both = MaterialSlots::GenerateTextureDefines(MaterialSlots::MaskBit(TextureSlot::DetailDiffuse) | MaterialSlots::MaskBit(TextureSlot::DetailNormal));
    CHECK(CountOf(both, "HAS_DETAIL_TEXTURES") == 1);                                    // once, not once per map
    CHECK(!Contains(MaterialSlots::GenerateTextureDefines(MaterialSlots::MaskBit(TextureSlot::Diffuse)), "PARALLAX"));

    // bits that belong to no slot are ignored, not turned into garbage
    CHECK(MaterialSlots::GenerateTextureDefines(0xFFFFC000u).empty());
    CHECK(MaterialSlots::GenerateTextureDefines(0x80000000u | 1u) == "#define HAS_DIFFUSE_TEXTURE 1\n");

    // every slot's define appears exactly once when every slot is set
    const std::string all = MaterialSlots::GenerateTextureDefines(0xFFFFFFFFu);
    for (const SlotInfo& r : MaterialSlots::All()) CHECK(CountOf(all, std::string("#define ") + r.define + " 1\n") == 1);
}

static void TestGeneratedDeclarations()
{
    const std::string text = MaterialSlots::GenerateHlslDeclarations();
    const auto lines = Lines(text);
    CHECK(lines.size() == kTextureSlotCount);
    CHECK(!text.empty() && text.back() == '\n');

    std::set<int> registers;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        const SlotInfo& r = MaterialSlots::All()[i];
        const std::string expect = std::string(r.hlslType) + " " + r.hlslName + " : register(t" + std::to_string(i) + ");";
        CHECK(lines[i].rfind(expect, 0) == 0);                                           // type, name and register t<slot>
        registers.insert(static_cast<int>(i));
    }
    CHECK(registers.size() == kTextureSlotCount);                                        // no register used twice
    CHECK(Contains(text, "TextureCube environmentTexture : register(t11);"));            // spot-check against the old hand-written file
    CHECK(Contains(text, "Texture2D brdfLUTTexture : register(t13);"));
}

static void TestMaskToString()
{
    CHECK(MaterialSlots::MaskToString(0) == "none");
    CHECK(MaterialSlots::MaskToString(MaterialSlots::MaskBit(TextureSlot::Normal) | MaterialSlots::MaskBit(TextureSlot::Diffuse)) == "Diffuse+Normal");
}

// ----------------------------------------------------------------------------------------- Material
static uint32_t MaskFromTextures(const Material& m)
{
    uint32_t mask = 0;
    const auto& t = m.GetTextures();
    for (uint32_t i = 0; i < t.size(); ++i) if (t[i]) mask |= 1u << i;
    return mask;
}

static void TestMaterialMaskFollowsTextures()
{
    Material m("m");
    CHECK(m.GetTextureMask() == 0 && m.GetTextures().size() == kTextureSlotCount);

    m.SetTexture(TextureSlot::Diffuse, Tex());
    CHECK(m.GetTextureMask() == MaterialSlots::MaskBit(TextureSlot::Diffuse));
    m.SetTexture(TextureSlot::Normal, Tex());
    CHECK(m.GetTextureMask() == (MaterialSlots::MaskBit(TextureSlot::Diffuse) | MaterialSlots::MaskBit(TextureSlot::Normal)));

    m.SetTexture(TextureSlot::Diffuse, Tex());                       // replacing keeps the bit
    CHECK(MaterialSlots::Has(m.GetTextureMask(), TextureSlot::Diffuse));

    m.SetTexture(TextureSlot::Diffuse, nullptr);                     // removing CLEARS it: the shader must stop sampling it
    CHECK(!MaterialSlots::Has(m.GetTextureMask(), TextureSlot::Diffuse) && m.GetTexture(TextureSlot::Diffuse) == nullptr);
    CHECK(MaterialSlots::Has(m.GetTextureMask(), TextureSlot::Normal));

    // every slot, one at a time: the bit is the slot's bit and nothing else
    for (const SlotInfo& r : MaterialSlots::All())
    {
        Material one("one");
        one.SetTexture(r.slot, Tex());
        CHECK(one.GetTextureMask() == MaterialSlots::MaskBit(r.slot));
        CHECK(one.GetTexture(r.slot) != nullptr);
    }
}

static void TestMaterialRejectsInvalidSlots()
{
    Material m("m");
    m.SetTexture(TextureSlot::Normal, Tex());
    const uint32_t before = m.GetTextureMask();
    m.SetTexture(TextureSlot::Count, Tex());                          // the sentinel is not a slot
    m.SetTexture(static_cast<TextureSlot>(14), Tex());                // the old, retired Shadow slot
    m.SetTexture(static_cast<TextureSlot>(500), Tex());
    CHECK(m.GetTextureMask() == before);
    CHECK(m.GetTexture(static_cast<TextureSlot>(500)) == nullptr && m.GetTexture(TextureSlot::Count) == nullptr);
    CHECK(m.GetTextures().size() == kTextureSlotCount);               // nothing grew
}

static void TestMaterialMaskNeverDrifts()
{
    // the bug class this milestone removes: a flag and a pointer that can disagree. Hammer it.
    std::mt19937 rng(1234);
    Material m("m");
    for (int i = 0; i < 5000; ++i)
    {
        const TextureSlot slot = static_cast<TextureSlot>(rng() % (kTextureSlotCount + 3));   // includes invalid slots
        if (rng() % 2) m.SetTexture(slot, Tex()); else m.SetTexture(slot, nullptr);
        CHECK(m.GetTextureMask() == MaskFromTextures(m));
    }
}

static void TestMaterialToDefinesEndToEnd()
{
    Material m("m");
    m.SetTexture(TextureSlot::Diffuse, Tex());
    m.SetTexture(TextureSlot::Height, Tex());
    const std::string defs = MaterialSlots::GenerateTextureDefines(m.GetTextureMask());
    CHECK(defs == "#define HAS_DIFFUSE_TEXTURE 1\n#define HAS_HEIGHT_MAP 1\n#define ENABLE_PARALLAX_MAPPING 1\n");
    m.SetTexture(TextureSlot::Height, nullptr);
    CHECK(MaterialSlots::GenerateTextureDefines(m.GetTextureMask()) == "#define HAS_DIFFUSE_TEXTURE 1\n");   // parallax switches off with the map
}


// ----------------------------------------------------------------------------------------- colour spaces + mips
static std::vector<uint8_t> Image(uint32_t w, uint32_t h, std::initializer_list<uint8_t> rgbaPerPixel)
{
    std::vector<uint8_t> px(rgbaPerPixel);
    px.resize(static_cast<size_t>(w) * h * 4, 255);
    return px;
}

static void TestSrgbConversion()
{
    CHECK(ImageMips::SrgbToLinear(0) == 0.0f);
    CHECK(std::fabs(ImageMips::SrgbToLinear(255) - 1.0f) < 1e-6f);
    CHECK(std::fabs(ImageMips::SrgbToLinear(188) - 0.5f) < 0.01f);              // "188" is the sRGB code of half the light
    CHECK(std::fabs(ImageMips::SrgbToLinear(128) - 0.2159f) < 0.001f);          // ... and 128 is only ~22%: the whole point
    CHECK(ImageMips::LinearToSrgb(0.0f) == 0 && ImageMips::LinearToSrgb(1.0f) == 255);
    CHECK(ImageMips::LinearToSrgb(-3.0f) == 0 && ImageMips::LinearToSrgb(7.0f) == 255);   // clamped, not wrapped
    CHECK(ImageMips::LinearToSrgb(0.5f) == 188);
    for (int i = 0; i < 256; ++i)                                                // exact inverses over the whole byte range
        CHECK(ImageMips::LinearToSrgb(ImageMips::SrgbToLinear(static_cast<uint8_t>(i))) == i);
    float prev = -1.0f;
    for (int i = 0; i < 256; ++i)                                                // strictly increasing: no code decodes lower than the one below
    {
        const float v = ImageMips::SrgbToLinear(static_cast<uint8_t>(i));
        CHECK(v > prev);
        prev = v;
    }
}

static void TestMipLevelCount()
{
    CHECK(ImageMips::MipLevelCount(1, 1) == 1);
    CHECK(ImageMips::MipLevelCount(2, 2) == 2);
    CHECK(ImageMips::MipLevelCount(256, 256) == 9);
    CHECK(ImageMips::MipLevelCount(1024, 256) == 11);        // follows the LARGER side
    CHECK(ImageMips::MipLevelCount(5, 3) == 3);
    CHECK(ImageMips::MipLevelCount(4096, 1) == 13);
}

static void TestMipChainLayout()
{
    // total size, level count, and the last level being exactly 1x1, for square / non-square / non-power-of-two images
    const uint32_t sizes[][2] = { { 1, 1 }, { 2, 2 }, { 4, 4 }, { 8, 2 }, { 1, 8 }, { 3, 3 }, { 5, 7 }, { 33, 17 }, { 64, 64 } };
    for (const auto& wh : sizes)
        for (TextureColorSpace cs : { TextureColorSpace::Linear, TextureColorSpace::SRGB })
        {
            const uint32_t w = wh[0], h = wh[1];
            std::vector<uint8_t> px(static_cast<size_t>(w) * h * 4, 77);
            const uint32_t levels = ImageMips::AppendMipChain(px, w, h, cs);
            CHECK(levels == ImageMips::MipLevelCount(w, h));
            size_t expect = 0; uint32_t lw = w, lh = h;
            for (uint32_t l = 0; l < levels; ++l) { expect += static_cast<size_t>(lw) * lh * 4; lw = std::max(1u, lw / 2); lh = std::max(1u, lh / 2); }
            CHECK(px.size() == expect);                                          // exactly the layout D3D11Texture reads
            CHECK(lw == 1 && lh == 1);
        }
}

static void TestSrgbMipsAverageLight()
{
    // 2x2 black / white checkerboard, one mip level.
    const auto checker = Image(2, 2, { 0, 0, 0, 255,   255, 255, 255, 255,   255, 255, 255, 255,   0, 0, 0, 255 });

    auto srgb = checker;   ImageMips::AppendMipChain(srgb, 2, 2, TextureColorSpace::SRGB);
    auto data = checker;   ImageMips::AppendMipChain(data, 2, 2, TextureColorSpace::Linear);
    const size_t m = 16;                                                         // level 1 starts after the 4 level-0 pixels

    // half the light of the checkerboard is sRGB 188; averaging the CODES would give 128 and a too-dark distance
    CHECK(srgb[m] == 188 && srgb[m + 1] == 188 && srgb[m + 2] == 188 && srgb[m + 3] == 255);
    // data is averaged as plain numbers
    CHECK(data[m] == 128 && data[m + 1] == 128 && data[m + 2] == 128 && data[m + 3] == 255);
}

static void TestAlphaIsNeverGammaEncoded()
{
    const auto img = Image(2, 2, { 10, 10, 10, 0,   10, 10, 10, 255,   10, 10, 10, 255,   10, 10, 10, 0 });
    auto srgb = img;   ImageMips::AppendMipChain(srgb, 2, 2, TextureColorSpace::SRGB);
    CHECK(srgb[16 + 3] == 128);                                                  // alpha: the plain average, even in an sRGB image
    CHECK(srgb[16] == 10);                                                       // a uniform colour stays exactly what it was
}

static void TestUniformImagesStayUniform()
{
    for (TextureColorSpace cs : { TextureColorSpace::Linear, TextureColorSpace::SRGB })
        for (int v : { 0, 1, 37, 128, 200, 254, 255 })
        {
            std::vector<uint8_t> px(8 * 8 * 4, static_cast<uint8_t>(v));
            const uint32_t levels = ImageMips::AppendMipChain(px, 8, 8, cs);
            for (size_t i = 0; i < px.size(); ++i) CHECK(px[i] == v);            // no drift at ANY level, in either space
            CHECK(levels == 4);
        }
}

static void TestFlatNormalMapSurvivesMips()
{
    // a flat normal map (128, 128, 255) must stay (128, 128, 255) down to 1x1, or distant normals tilt
    std::vector<uint8_t> px;
    for (int i = 0; i < 16 * 16; ++i) { px.push_back(128); px.push_back(128); px.push_back(255); px.push_back(255); }
    ImageMips::AppendMipChain(px, 16, 16, TextureColorSpace::Linear);
    for (size_t i = 0; i < px.size(); i += 4) CHECK(px[i] == 128 && px[i + 1] == 128 && px[i + 2] == 255 && px[i + 3] == 255);
}

static void TestSrgbMipsAreBrighterThanNaive()
{
    // On a random image the linear-light mip is never darker than the naive byte average (a concave function): checked per pixel.
    std::mt19937 rng(9);
    std::vector<uint8_t> a(32 * 32 * 4);
    for (auto& b : a) b = static_cast<uint8_t>(rng());
    auto correct = a, naive = a;
    ImageMips::AppendMipChain(correct, 32, 32, TextureColorSpace::SRGB);
    ImageMips::AppendMipChain(naive,   32, 32, TextureColorSpace::Linear);
    const size_t level1 = 32 * 32 * 4;
    int brighter = 0;
    for (size_t i = 0; i < 16 * 16 * 4; ++i)
    {
        if ((i % 4) == 3) continue;                                              // alpha is identical in both
        CHECK(correct[level1 + i] + 1 >= naive[level1 + i]);                     // never meaningfully darker (+1 for rounding)
        brighter += correct[level1 + i] > naive[level1 + i];
    }
    CHECK(brighter > 200);                                                       // and usually brighter: the test has teeth
}

int main()
{
    struct { const char* name; void (*fn)(); } tests[] = {
        { "slots: one row per slot, row i == slot i == register ti", TestTableShape },
        { "slots: names, HLSL names and defines are unique + valid", TestTableNamesAreUniqueAndValid },
        { "slots: engine-wide registers never overlap them",        TestEngineRegistersStayClear },
        { "slots: colours are sRGB, data is linear",                TestColourSpaceRule },
        { "slots: mask bits",                                       TestMask },
        { "slots: generated #defines",                              TestGeneratedDefines },
        { "slots: generated HLSL declarations",                     TestGeneratedDeclarations },
        { "slots: mask to string",                                  TestMaskToString },
        { "material: mask follows the textures (set / replace / remove)", TestMaterialMaskFollowsTextures },
        { "material: invalid slots are rejected",                   TestMaterialRejectsInvalidSlots },
        { "material: mask never drifts (5000 random operations)",   TestMaterialMaskNeverDrifts },
        { "material: texture -> mask -> #defines",                  TestMaterialToDefinesEndToEnd },
        { "mips: sRGB conversion is exact and invertible",          TestSrgbConversion },
        { "mips: level counts",                                     TestMipLevelCount },
        { "mips: chain layout (square, non-square, odd sizes)",     TestMipChainLayout },
        { "mips: sRGB averages light (checkerboard -> 188, data -> 128)", TestSrgbMipsAverageLight },
        { "mips: alpha is never gamma-encoded",                     TestAlphaIsNeverGammaEncoded },
        { "mips: uniform images do not drift",                      TestUniformImagesStayUniform },
        { "mips: a flat normal map stays flat",                     TestFlatNormalMapSurvivesMips },
        { "mips: sRGB mips are brighter than naive byte averages",  TestSrgbMipsAreBrighterThanNaive },
    };
    for (auto& t : tests)
    {
        const int before = g_failed;
        t.fn();
        std::printf("%s  %s\n", g_failed == before ? "[ ok ]" : "[FAIL]", t.name);
    }
    std::printf("\n%s\n", g_failed == 0 ? "all material tests passed" : "SOME MATERIAL TESTS FAILED");
    return g_failed == 0 ? 0 : 1;
}
