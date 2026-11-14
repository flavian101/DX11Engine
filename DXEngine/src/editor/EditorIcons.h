#pragma once
#include "imgui.h"

namespace DXEngine
{
    // ==================================================================
    // EditorIcons: small vector icons drawn with the draw list. No font, no texture, no asset to load, and they stay sharp at
    // any UI scale. Every icon fits a square of 'size' pixels centred on 'center'.
    // ==================================================================
    enum class Icon
    {
        Entity, Mesh, Light, Sun, Spot, Sky,                  // what an entity is
        Select, Move, Rotate, Scale,                          // tools
        Play, Stop,
        Eye, EyeOff, Search, Close, Plus, Wireframe, Shadow   // actions
    };

    namespace EditorIcons
    {
        void Draw(ImDrawList* list, Icon icon, ImVec2 center, float size, ImU32 color);

        // A square button with an icon. 'active' = drawn as pressed (the current tool). Returns true when clicked.
        bool Button(const char* id, Icon icon, bool active = false, const char* tooltip = nullptr, float size = 0.0f);

        // The icon and colour that stand for an entity in lists.
        struct Kind { Icon icon; ImVec4 color; };
    }
}
