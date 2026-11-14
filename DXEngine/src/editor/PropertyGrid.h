#pragma once
#include <DirectXMath.h>

namespace DXEngine
{
    // ==================================================================
    // PropertyGrid: "label on the left, control on the right", one row per property, every panel the same way.
    //
    //     if (PropertyGrid::Begin("transform"))
    //     {
    //         PropertyGrid::Vec3("Position", t.position);
    //         PropertyGrid::Float("Intensity", light.intensity, 0.05f, 0.0f, 100.0f);
    //         PropertyGrid::End();
    //     }
    //
    // Every control returns true in the frame its value was edited. Labels are also the ids, so they must be unique inside
    // one grid. The label column's width is shared by all grids and can be dragged.
    // ==================================================================
    namespace PropertyGrid
    {
        bool Begin(const char* id);
        void End();

        // Starts a row and draws its label (use for custom controls; the helpers below do it themselves).
        void Label(const char* label, const char* tooltip = nullptr);

        // x / y / z with the axis letters as buttons: a click on a letter sets that component to 'reset'.
        bool Vec3(const char* label, DirectX::XMFLOAT3& value, float speed = 0.05f,
                  DirectX::XMFLOAT3 reset = { 0.0f, 0.0f, 0.0f }, const char* format = "%.2f");

        bool Float(const char* label, float& value, float speed = 0.05f, float min = 0.0f, float max = 0.0f, const char* format = "%.2f");
        bool Slider(const char* label, float& value, float min, float max, const char* format = "%.2f");
        bool Checkbox(const char* label, bool& value);
        bool Color3(const char* label, float* rgb, bool hdr = false);
        bool Color4(const char* label, float* rgba);
        bool Combo(const char* label, int& index, const char* const* items, int count);

        // a read-only value
        void Text(const char* label, const char* format, ...);
    }
}
