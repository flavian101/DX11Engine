#pragma once
#include <DirectXMath.h>
#include <cmath>

namespace DXEngine
{
    // ==================================================================
    // Pure maths of the editor's viewport panel (no ImGui, no GPU): tested in editor_tests.
    // ==================================================================
    namespace ViewportMath
    {
        struct Size { int width = 0; int height = 0; };

        // The scene texture size for a panel whose content area is (availableWidth x availableHeight) pixels.
        // Whole pixels (fractions are cut), and a panel that is collapsed / not laid out yet (< 1 pixel) keeps the
        // previous size: asking the renderer for 0 x 0 would draw the scene into the window behind the panels.
        // With no previous size the minimum is 1 x 1.
        inline Size ChooseTargetSize(float availableWidth, float availableHeight, Size previous)
        {
            const int w = static_cast<int>(std::floor(availableWidth));
            const int h = static_cast<int>(std::floor(availableHeight));
            if (w >= 1 && h >= 1) return { w, h };
            if (previous.width >= 1 && previous.height >= 1) return previous;
            return { 1, 1 };
        }

        inline float AspectRatio(int width, int height)
        {
            return (width > 0 && height > 0) ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
        }

        // A mouse position in screen pixels -> a pixel of the scene texture, for picking. The image on screen is
        // (imageWidth x imageHeight) at (imageX, imageY); the texture is (targetWidth x targetHeight): they differ for a
        // frame after the panel is resized, so the position is scaled. Returns false when the mouse is outside the image.
        inline bool ScreenToTarget(float mouseX, float mouseY,
                                   float imageX, float imageY, float imageWidth, float imageHeight,
                                   int targetWidth, int targetHeight,
                                   float& outX, float& outY)
        {
            if (imageWidth <= 0.0f || imageHeight <= 0.0f || targetWidth <= 0 || targetHeight <= 0) return false;
            const float u = (mouseX - imageX) / imageWidth;
            const float v = (mouseY - imageY) / imageHeight;
            if (u < 0.0f || u >= 1.0f || v < 0.0f || v >= 1.0f) return false;
            outX = u * static_cast<float>(targetWidth);
            outY = v * static_cast<float>(targetHeight);
            return true;
        }

        // Where a new entity appears: straight ahead of the camera, so it is on screen and not inside the ground or the camera.
        inline DirectX::XMFLOAT3 PointInFront(const DirectX::XMFLOAT3& cameraPosition, const DirectX::XMFLOAT3& cameraForward, float distance)
        {
            return { cameraPosition.x + cameraForward.x * distance,
                     cameraPosition.y + cameraForward.y * distance,
                     cameraPosition.z + cameraForward.z * distance };
        }

        // Fly speed after 'notches' of mouse wheel (positive = faster): 20 % per notch, kept between 0.5 and 400.
        inline float AdjustFlySpeed(float speed, float notches)
        {
            const float adjusted = speed * std::pow(1.2f, notches);
            return adjusted < 0.5f ? 0.5f : (adjusted > 400.0f ? 400.0f : adjusted);
        }

        // A world point -> pixels inside a viewport of (width x height) (origin top-left, y down).
        // Matrices are row-vector (clip = world * view * projection). Returns false for a point behind the camera.
        inline bool ProjectToViewport(const DirectX::XMFLOAT3& world,
                                      const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection,
                                      float width, float height, DirectX::XMFLOAT2& out)
        {
            using namespace DirectX;
            const XMVECTOR clip = XMVector4Transform(XMVectorSet(world.x, world.y, world.z, 1.0f),
                                                     XMLoadFloat4x4(&view) * XMLoadFloat4x4(&projection));
            const float w = XMVectorGetW(clip);
            if (w <= 1e-6f) return false;
            const float ndcX = XMVectorGetX(clip) / w;
            const float ndcY = XMVectorGetY(clip) / w;
            out.x = (ndcX * 0.5f + 0.5f) * width;
            out.y = (1.0f - (ndcY * 0.5f + 0.5f)) * height;
            return true;
        }
    }
}
