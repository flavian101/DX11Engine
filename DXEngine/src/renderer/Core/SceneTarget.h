#pragma once
#include <algorithm>
#include <cstdint>

namespace DXEngine
{
    // ==================================================================
    // SceneTargetSize: how big the scene texture is. Pure arithmetic, so it is tested without a GPU.
    //
    // The scene texture is what an editor shows inside a panel. The panel's size is whatever the user dragged it to
    // (fractional pixels, zero while it is collapsed, larger than any GPU supports if the window is huge), so the
    // request is turned into a size that can actually be created:
    //   * either side <= 0       -> disabled (width = height = 0): the scene is drawn into the back buffer
    //   * otherwise              -> each side clamped to [1, maxSize]
    // ==================================================================
    struct SceneTargetSize
    {
        uint32_t width  = 0;
        uint32_t height = 0;

        bool Enabled() const { return width > 0 && height > 0; }
        bool operator==(const SceneTargetSize& o) const { return width == o.width && height == o.height; }
        bool operator!=(const SceneTargetSize& o) const { return !(*this == o); }
    };

    inline SceneTargetSize ResolveSceneTargetSize(int requestedWidth, int requestedHeight, uint32_t maxSize)
    {
        if (requestedWidth <= 0 || requestedHeight <= 0 || maxSize == 0)
            return {};
        const uint32_t w = std::min<uint32_t>(static_cast<uint32_t>(requestedWidth),  maxSize);
        const uint32_t h = std::min<uint32_t>(static_cast<uint32_t>(requestedHeight), maxSize);
        return { std::max(1u, w), std::max(1u, h) };
    }
}
