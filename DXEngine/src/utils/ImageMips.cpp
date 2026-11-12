#include "ImageMips.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace DXEngine
{
    namespace ImageMips
    {
        namespace
        {
            const std::array<float, 256>& DecodeTable()
            {
                static const std::array<float, 256> table = []
                {
                    std::array<float, 256> t{};
                    for (int i = 0; i < 256; ++i)
                    {
                        const float c = static_cast<float>(i) / 255.0f;
                        t[i] = c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
                    }
                    return t;
                }();
                return table;
            }
        }

        float SrgbToLinear(uint8_t encoded) { return DecodeTable()[encoded]; }

        uint8_t LinearToSrgb(float linear)
        {
            const float l = std::clamp(linear, 0.0f, 1.0f);
            const float c = l <= 0.0031308f ? l * 12.92f : 1.055f * std::pow(l, 1.0f / 2.4f) - 0.055f;
            return static_cast<uint8_t>(std::clamp(c * 255.0f + 0.5f, 0.0f, 255.0f));
        }

        uint32_t MipLevelCount(uint32_t width, uint32_t height)
        {
            uint32_t levels = 1;
            for (uint32_t size = std::max(width, height); size > 1; size /= 2) ++levels;
            return levels;
        }

        uint32_t AppendMipChain(std::vector<uint8_t>& pixels, uint32_t width, uint32_t height, TextureColorSpace colorSpace)
        {
            const bool srgb = colorSpace == TextureColorSpace::SRGB;
            uint32_t levels = 1;
            size_t prevOffset = 0;
            uint32_t pw = width, ph = height;

            while (pw > 1 || ph > 1)
            {
                const uint32_t nw = std::max(1u, pw / 2);
                const uint32_t nh = std::max(1u, ph / 2);

                const size_t newOffset = pixels.size();
                pixels.resize(newOffset + static_cast<size_t>(nw) * nh * 4);   // may reallocate: index, never hold pointers

                for (uint32_t y = 0; y < nh; ++y)
                {
                    const uint32_t y0 = std::min(2 * y,     ph - 1);
                    const uint32_t y1 = std::min(2 * y + 1, ph - 1);
                    for (uint32_t x = 0; x < nw; ++x)
                    {
                        const uint32_t x0 = std::min(2 * x,     pw - 1);
                        const uint32_t x1 = std::min(2 * x + 1, pw - 1);

                        for (uint32_t c = 0; c < 4; ++c)
                        {
                            auto at = [&](uint32_t sx, uint32_t sy) -> uint8_t {
                                return pixels[prevOffset + (static_cast<size_t>(sy) * pw + sx) * 4 + c];
                            };
                            uint8_t result;
                            if (srgb && c < 3)       // colour channels of an sRGB image: average the LIGHT, not the code values
                                result = LinearToSrgb(0.25f * (SrgbToLinear(at(x0, y0)) + SrgbToLinear(at(x1, y0)) +
                                                               SrgbToLinear(at(x0, y1)) + SrgbToLinear(at(x1, y1))));
                            else                     // data, and alpha of everything: the plain average (rounded)
                                result = static_cast<uint8_t>((static_cast<uint32_t>(at(x0, y0)) + at(x1, y0) + at(x0, y1) + at(x1, y1) + 2) / 4);
                            pixels[newOffset + (static_cast<size_t>(y) * nw + x) * 4 + c] = result;
                        }
                    }
                }

                prevOffset = newOffset;
                pw = nw;
                ph = nh;
                ++levels;
            }
            return levels;
        }
    }
}
