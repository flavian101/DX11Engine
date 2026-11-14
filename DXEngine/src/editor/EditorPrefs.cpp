#include "dxpch.h"
#include "EditorPrefs.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace DXEngine
{
    namespace
    {
        // strict float: the whole text must be a number (strtof alone would accept "1.5abc")
        bool ParseFloat(const std::string& text, float& out)
        {
            if (text.empty()) return false;
            char* end = nullptr;
            const float v = std::strtof(text.c_str(), &end);
            if (end == text.c_str() || *end != '\0') return false;
            if (!(v == v) || v > 1e9f || v < -1e9f) return false;       // NaN / absurd
            out = v;
            return true;
        }

        std::string Trim(const std::string& s)
        {
            const size_t a = s.find_first_not_of(" \t\r\n");
            if (a == std::string::npos) return std::string();
            const size_t b = s.find_last_not_of(" \t\r\n");
            return s.substr(a, b - a + 1);
        }
    }

    EditorPrefs ParseEditorPrefs(const std::string& text)
    {
        EditorPrefs prefs;
        std::istringstream in(text);
        std::string line;
        while (std::getline(in, line))
        {
            const size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            const std::string key = Trim(line.substr(0, eq));
            float v = 0.0f;
            if (!ParseFloat(Trim(line.substr(eq + 1)), v)) continue;
            if (key == "ui_scale"  && v >= 0.75f && v <= 2.5f)   prefs.uiScale = v;
            if (key == "fly_speed" && v >= 0.5f  && v <= 400.0f) prefs.flySpeed = v;
        }
        return prefs;
    }

    std::string FormatEditorPrefs(const EditorPrefs& prefs)
    {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "ui_scale=%.3f\nfly_speed=%.3f\n", prefs.uiScale, prefs.flySpeed);
        return buffer;
    }

    EditorPrefs LoadEditorPrefs(const char* path)
    {
        std::ifstream file(path);
        if (!file) return EditorPrefs();
        std::stringstream text;
        text << file.rdbuf();
        return ParseEditorPrefs(text.str());
    }

    void SaveEditorPrefs(const EditorPrefs& prefs, const char* path)
    {
        std::ofstream file(path, std::ios::trunc);
        if (file) file << FormatEditorPrefs(prefs);
    }
}
