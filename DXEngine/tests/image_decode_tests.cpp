// ImageDecode: image files -> 8-bit pixels. CPU only. Fixtures in tests/data (small EXR files in several compressions).
#include "utils/ImageDecode.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

using namespace DXEngine;
using namespace DXEngine::ImageDecode;
namespace fs = std::filesystem;

#ifndef DX_TEST_DATA_DIR
#define DX_TEST_DATA_DIR "tests/data"
#endif
#ifndef DX_ASSETS_DIR
#define DX_ASSETS_DIR "Editor/assets"
#endif

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_failures; } } while (0)
static bool Contains(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }
static bool NearByte(int a, int b, int eps = 1) { return std::abs(a - b) <= eps; }
static std::string Data(const char* name) { return std::string(DX_TEST_DATA_DIR) + "/" + name; }

static void SrgbEncoding()
{
    CHECK(EncodeSRGB(0.0f) == 0 && EncodeSRGB(1.0f) == 255);
    CHECK(EncodeSRGB(0.5f) == 188);                              // the real curve: 0.5 linear is 188, not 128
    CHECK(EncodeSRGB(0.2f) == 124);
    CHECK(EncodeSRGB(0.0031308f) == 10 && EncodeSRGB(0.001f) == 3);   // the linear toe
    CHECK(EncodeSRGB(-3.0f) == 0 && EncodeSRGB(7.0f) == 255);    // out of range is clamped
    for (int i = 1; i <= 255; ++i) CHECK(EncodeSRGB(i / 255.0f) >= EncodeSRGB((i - 1) / 255.0f));   // monotonic
}

static void ExrInSeveralCompressions()
{
    // single channel, ZIP: the grey value goes to R, G and B; u runs 0..1 over 8 columns
    Image y = DecodeFile(Data("y_zip.exr"), TextureColorSpace::Linear);
    CHECK(y.ok() && y.width == 8 && y.height == 8 && y.hdrSource);
    if (y.ok())
    {
        CHECK(y.rgba[0] == 0 && y.rgba[1] == 0 && y.rgba[2] == 0);                            // x = 0
        const size_t last = 7 * 4;                                                           // x = 7: 1.0
        CHECK(y.rgba[last] == 255 && y.rgba[last + 1] == 255 && y.rgba[last + 2] == 255 && y.rgba[last + 3] == 255);
        CHECK(NearByte(y.rgba[3 * 4], 109, 2));                                              // x = 3: 3/7 = 0.4286 -> 109 (linear: stored as is)
        const Image ys = DecodeFile(Data("y_zip.exr"), TextureColorSpace::SRGB);
        CHECK(NearByte(ys.rgba[3 * 4], EncodeSRGB(3.0f / 7.0f), 2) && ys.rgba[3 * 4] > y.rgba[3 * 4]);   // colour maps are sRGB encoded
    }

    // RGB, PIZ, with an HDR corner (4.0): clamped to white, the rest as authored
    Image rgb = DecodeFile(Data("rgb_piz.exr"), TextureColorSpace::Linear);
    CHECK(rgb.ok() && rgb.width == 8 && rgb.height == 8);
    if (rgb.ok())
    {
        CHECK(rgb.rgba[0] == 255 && rgb.rgba[1] == 255 && rgb.rgba[2] == 255);               // the 4.0 pixel
        const size_t p = (3 * 8 + 5) * 4;                                                    // row 3, column 5: R = 5/7, G = 3/7, B = 0.5
        CHECK(NearByte(rgb.rgba[p], 182, 2) && NearByte(rgb.rgba[p + 1], 109, 2) && NearByte(rgb.rgba[p + 2], 128, 1));
    }

    // RGBA, uncompressed: alpha is never gamma encoded
    Image rgba = DecodeFile(Data("rgba_none.exr"), TextureColorSpace::SRGB);
    CHECK(rgba.ok() && NearByte(rgba.rgba[0], 124, 1) && NearByte(rgba.rgba[3], 64, 1));
    Image lin = DecodeFile(Data("rgba_none.exr"), TextureColorSpace::Linear);
    CHECK(NearByte(lin.rgba[0], 51, 1) && NearByte(lin.rgba[3], 64, 1));
}

static void DwaaIsRefusedWithAClearMessage()
{
    // the lossy DWAA compression is not supported: the decode FAILS and says why, never hands back garbage
    Image dwaa = DecodeFile(Data("rgb_dwaa.exr"), TextureColorSpace::Linear);
    CHECK(!dwaa.ok() && dwaa.rgba.empty());
    CHECK(Contains(dwaa.error, "DWAA") && Contains(dwaa.error, "Re-export") && Contains(dwaa.error, "rgb_dwaa.exr"));

    // the real maps of the sample lion are DWAA too
    const std::string lion = std::string(DX_ASSETS_DIR) + "/models/lion/textures/lion_head_rough_2k.exr";
    if (fs::exists(lion))
    {
        Image real = DecodeFile(lion, TextureColorSpace::Linear);
        CHECK(!real.ok() && Contains(real.error, "DWAA"));
    }
}

static std::vector<uint8_t> Slurp(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

static void ExrCompressionNames()
{
    CHECK(ExrCompressionName(Slurp(Data("y_zip.exr")).data(), Slurp(Data("y_zip.exr")).size()) == "ZIP");
    const auto piz = Slurp(Data("rgb_piz.exr")), none = Slurp(Data("rgba_none.exr")), dwaa = Slurp(Data("rgb_dwaa.exr"));
    CHECK(ExrCompressionName(piz.data(), piz.size()) == "PIZ");
    CHECK(ExrCompressionName(none.data(), none.size()) == "NONE");
    CHECK(ExrCompressionName(dwaa.data(), dwaa.size()) == "DWAA");
    const uint8_t png[] = { 0x89, 'P', 'N', 'G', 0, 0, 0, 0 };
    CHECK(ExrCompressionName(png, sizeof(png)).empty());                 // not an EXR
    CHECK(ExrCompressionName(nullptr, 0).empty());
}

static void WriteBmp(const fs::path& path)
{
    // 2x2, 24 bit, bottom row first, BGR: bottom-left red, bottom-right green, top-left blue, top-right white
    const uint8_t pixels[] = { 0,0,255,  0,255,0, 0,0,  255,0,0,  255,255,255, 0,0 };   // rows padded to 4 bytes
    uint8_t header[54] = { 'B','M', 0,0,0,0, 0,0,0,0, 54,0,0,0, 40,0,0,0, 2,0,0,0, 2,0,0,0, 1,0, 24,0 };
    const uint32_t size = 54 + sizeof(pixels);
    std::memcpy(header + 2, &size, 4);
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(header), 54);
    out.write(reinterpret_cast<const char*>(pixels), sizeof(pixels));
}

static void StbFormatsAndFailures()
{
    const fs::path bmp = fs::temp_directory_path() / "dx_decode_test.bmp";
    WriteBmp(bmp);
    Image img = DecodeFile(bmp.string(), TextureColorSpace::SRGB);
    CHECK(img.ok() && img.width == 2 && img.height == 2 && !img.hdrSource);
    if (img.ok())
    {
        // top-left blue, top-right white, bottom-left red, bottom-right green: 8-bit files are returned AS STORED (no re-encoding)
        CHECK(img.rgba[0] == 0 && img.rgba[1] == 0 && img.rgba[2] == 255 && img.rgba[3] == 255);
        CHECK(img.rgba[4] == 255 && img.rgba[5] == 255 && img.rgba[6] == 255);
        CHECK(img.rgba[8] == 255 && img.rgba[9] == 0 && img.rgba[10] == 0);
        CHECK(img.rgba[12] == 0 && img.rgba[13] == 255 && img.rgba[14] == 0);
    }
    fs::remove(bmp);

    Image missing = DecodeFile((fs::temp_directory_path() / "dx_no_such_image.png").string(), TextureColorSpace::Linear);
    CHECK(!missing.ok() && Contains(missing.error, "cannot be opened"));

    const fs::path junk = fs::temp_directory_path() / "dx_decode_junk.png";
    std::ofstream(junk, std::ios::binary) << "this is not an image at all, just some text";
    Image bad = DecodeFile(junk.string(), TextureColorSpace::Linear);
    CHECK(!bad.ok() && Contains(bad.error, "dx_decode_junk.png"));
    fs::remove(junk);

    const fs::path empty = fs::temp_directory_path() / "dx_decode_empty.png";
    std::ofstream(empty, std::ios::binary).close();
    CHECK(!DecodeFile(empty.string(), TextureColorSpace::Linear).ok());
    fs::remove(empty);

    CHECK(!DecodeMemory(nullptr, 0, "none", TextureColorSpace::Linear).ok());
    const uint8_t trunc[] = { 0x76, 0x2f, 0x31, 0x01, 2, 0, 0, 0 };           // an EXR header that stops right away
    CHECK(!DecodeMemory(trunc, sizeof(trunc), "truncated.exr", TextureColorSpace::Linear).ok());
}

static void RadianceHdr()
{
    // 2x2 flat (uncompressed) Radiance file: RGBE pixels (128,64,32,129) = (1.0, 0.5, 0.25) and a dark one
    const fs::path hdr = fs::temp_directory_path() / "dx_decode_test.hdr";
    {
        std::ofstream out(hdr, std::ios::binary);
        out << "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 2 +X 2\n";
        const uint8_t px[] = { 128,64,32,129,  128,64,32,129,  0,0,0,0,  128,128,128,129 };
        out.write(reinterpret_cast<const char*>(px), sizeof(px));
    }
    Image lin = DecodeFile(hdr.string(), TextureColorSpace::Linear);
    CHECK(lin.ok() && lin.hdrSource && lin.width == 2 && lin.height == 2);
    if (lin.ok())
    {
        CHECK(lin.rgba[0] == 255 && NearByte(lin.rgba[1], 128, 1) && NearByte(lin.rgba[2], 64, 1) && lin.rgba[3] == 255);
        CHECK(lin.rgba[8] == 0 && lin.rgba[9] == 0 && lin.rgba[10] == 0);        // the black pixel
        CHECK(lin.rgba[12] == 255);                                              // (1,1,1)
    }
    Image srgb = DecodeFile(hdr.string(), TextureColorSpace::SRGB);
    CHECK(srgb.ok() && NearByte(srgb.rgba[1], 188, 1) && NearByte(srgb.rgba[2], 137, 1));
    fs::remove(hdr);
}

int main()
{
    SrgbEncoding();
    ExrInSeveralCompressions();
    DwaaIsRefusedWithAClearMessage();
    ExrCompressionNames();
    StbFormatsAndFailures();
    RadianceHdr();
    if (g_failures) { std::printf("%d check(s) failed\n", g_failures); return EXIT_FAILURE; }
    std::printf("image_decode_tests: all passed\n");
    return EXIT_SUCCESS;
}
