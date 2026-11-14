#pragma once
#include <string>

namespace DXEngine
{
    // ==================================================================
    // EditorPrefs: the few settings that outlive a run (not the window layout: ImGui keeps that in editor_layout.ini).
    // Stored as "key=value" lines in editor_prefs.ini next to the executable. Parsing is forgiving: unknown keys, junk lines
    // and out-of-range values are ignored and the default stays, so a hand-edited or old file can never break start-up.
    // ==================================================================
    struct EditorPrefs
    {
        float uiScale = 1.0f;          // View > UI scale (0.75 .. 2.5)
        float flySpeed = 20.0f;        // WASD speed of the camera (0.5 .. 400)
    };

    EditorPrefs ParseEditorPrefs(const std::string& text);
    std::string FormatEditorPrefs(const EditorPrefs& prefs);

    // file access; a missing file gives the defaults, a failed write is ignored (preferences are not worth an error)
    EditorPrefs LoadEditorPrefs(const char* path = "editor_prefs.ini");
    void SaveEditorPrefs(const EditorPrefs& prefs, const char* path = "editor_prefs.ini");
}
