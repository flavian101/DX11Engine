#pragma once
#include <string>

namespace DXEngine
{
    // ==================================================================
    // SceneIO: scene text <-> a file. Nothing else: no scene, no assets, no UI (SceneSerializer makes the text; the
    // controller decides when to call this).
    //
    // WriteTextFile never leaves a half-written file where a good one was: it writes "<path>.tmp" next to it, and only when
    // that is complete replaces the target. A crash, a full disk or a locked file therefore costs the new save, never the old one.
    // ==================================================================
    namespace SceneIO
    {
        constexpr const char* kExtension = ".dxscene";
        constexpr size_t kMaxFileBytes = 256u * 1024u * 1024u;       // a scene file bigger than this is not read (a wrong file chosen)

        struct IoResult
        {
            bool ok = false;
            std::string error;        // empty when ok
        };

        IoResult WriteTextFile(const std::string& path, const std::string& text);
        IoResult ReadTextFile(const std::string& path, std::string& text);        // 'text' is left empty on failure

        // "scene" -> "scene.dxscene"; a name that already has an extension is left alone ("level.json" stays).
        std::string WithSceneExtension(const std::string& path);
    }
}
