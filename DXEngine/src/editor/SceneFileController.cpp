#include "dxpch.h"
#include "SceneFileController.h"
#include "SceneIO.h"
#include "scene/TransformSystem.h"
#include "imgui.h"
#include <filesystem>

namespace DXEngine
{
    std::string SceneFileController::CurrentText(EditorContext& ctx)
    {
        if (!ctx.scene || !m_Hooks.makeBridge) return {};
        auto bridge = m_Hooks.makeBridge();
        return bridge ? SceneSerializer::Save(*ctx.scene, *bridge).text : std::string();
    }

    void SceneFileController::RememberBaseline(EditorContext& ctx)
    {
        m_Baseline = CurrentText(ctx);
        m_HasBaseline = true;
    }

    bool SceneFileController::IsModified(EditorContext& ctx)
    {
        if (!ctx.scene) return false;
        if (!m_HasBaseline) return true;                    // never saved, never loaded: built in code
        return CurrentText(ctx) != m_Baseline;
    }

    // ---------------------------------------------------------------- commands
    void SceneFileController::New(EditorContext& ctx)
    {
        if (!ctx.scene) return;
        if (IsModified(ctx)) { m_Pending = Action::New; m_OpenPopup = true; return; }
        Proceed(ctx, Action::New);
    }

    void SceneFileController::Open(EditorContext& ctx)
    {
        if (!ctx.scene) return;
        if (IsModified(ctx)) { m_Pending = Action::Open; m_OpenPopup = true; return; }
        Proceed(ctx, Action::Open);
    }

    bool SceneFileController::Save(EditorContext& ctx)
    {
        if (!ctx.scene) return false;
        return m_Path.empty() ? SaveAs(ctx) : WriteTo(ctx, m_Path);
    }

    bool SceneFileController::SaveAs(EditorContext& ctx)
    {
        if (!ctx.scene || !m_Hooks.pickSavePath) return false;
        const std::string suggested = m_Path.empty() ? std::string("scene") + SceneIO::kExtension : m_Path;
        const std::string chosen = m_Hooks.pickSavePath(suggested);
        if (chosen.empty()) return false;                   // cancelled: nothing happened
        return WriteTo(ctx, SceneIO::WithSceneExtension(chosen));
    }

    void SceneFileController::Proceed(EditorContext& ctx, Action action)
    {
        m_Pending = Action::None;
        if (action == Action::New)
        {
            ctx.scene->Clear();
            ctx.selection.Clear();
            m_Path.clear();
            RememberBaseline(ctx);
            Report("new scene", false);
        }
        else if (action == Action::Open)
        {
            if (!m_Hooks.pickOpenPath) return;
            const std::string chosen = m_Hooks.pickOpenPath();
            if (!chosen.empty()) ReadFrom(ctx, chosen);
        }
    }

    bool SceneFileController::WriteTo(EditorContext& ctx, const std::string& path)
    {
        if (!m_Hooks.makeBridge) return false;
        auto bridge = m_Hooks.makeBridge();
        if (!bridge) return false;

        const SceneSerializer::SaveResult saved = SceneSerializer::Save(*ctx.scene, *bridge);
        const SceneIO::IoResult io = SceneIO::WriteTextFile(path, saved.text);
        if (!io.ok) { Report("save failed: " + io.error, true); return false; }     // the old file, if any, is intact

        for (const std::string& w : saved.warnings) Report("save: " + w, true);
        m_Path = path;
        m_Baseline = saved.text;
        m_HasBaseline = true;
        Report("saved " + std::to_string(ctx.scene->EntityCount()) + " entities to '" + path + "'" +
               (saved.warnings.empty() ? std::string() : " (" + std::to_string(saved.warnings.size()) + " warning" + (saved.warnings.size() == 1 ? "" : "s") + ")"), false);
        return true;
    }

    void SceneFileController::ReadFrom(EditorContext& ctx, const std::string& path)
    {
        if (!m_Hooks.makeBridge) return;
        std::string text;
        const SceneIO::IoResult io = SceneIO::ReadTextFile(path, text);
        if (!io.ok) { Report("open failed: " + io.error, true); return; }

        auto bridge = m_Hooks.makeBridge();
        if (!bridge) return;
        const SceneSerializer::LoadResult r = SceneSerializer::Load(text, *ctx.scene, *bridge);
        if (!r.ok) { Report("open failed: " + r.error, true); return; }                  // the scene is untouched

        for (const std::string& w : r.warnings) Report("open: " + w, true);
        ctx.selection.Clear();                                                          // its entity is not in the loaded scene
        TransformSystem::Update(*ctx.scene);                                            // world matrices are not saved
        m_Path = path;
        RememberBaseline(ctx);                                                          // what a save would write now
        Report("opened '" + path + "': " + std::to_string(r.entities) + " entities" +
               (r.warnings.empty() ? std::string() : ", " + std::to_string(r.warnings.size()) + " warning" + (r.warnings.size() == 1 ? "" : "s")), false);
    }

    // ---------------------------------------------------------------- the question
    void SceneFileController::OnImGui(EditorContext& ctx)
    {
        if (m_Pending == Action::None) return;
        if (m_OpenPopup) { ImGui::OpenPopup("Unsaved changes"); m_OpenPopup = false; }

        bool open = true;
        if (!ImGui::BeginPopupModal("Unsaved changes", &open, ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (!open) m_Pending = Action::None;           // closed with the X: the same as Cancel
            return;
        }

        const std::string name = m_Path.empty() ? std::string("The scene") : "'" + std::filesystem::path(m_Path).filename().string() + "'";
        ImGui::Text("%s has changes that are not saved.", name.c_str());
        ImGui::Spacing();
        Choice choice = Choice::Cancel;
        bool answered = false;
        if (ImGui::Button("Save"))       { choice = Choice::Save;     answered = true; }
        ImGui::SameLine();
        if (ImGui::Button("Don't save")) { choice = Choice::DontSave; answered = true; }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))     { choice = Choice::Cancel;   answered = true; }
        if (answered) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        if (answered) AnswerPending(ctx, choice);          // after EndPopup: the answer may open a file dialog or replace the scene
    }

    void SceneFileController::AnswerPending(EditorContext& ctx, Choice choice)
    {
        if (m_Pending == Action::None) return;
        const Action action = m_Pending;
        switch (choice)
        {
        case Choice::Save:
            if (Save(ctx)) Proceed(ctx, action);
            else m_Pending = Action::None;                 // cancelled or failed: stay where we are
            break;
        case Choice::DontSave:
            Proceed(ctx, action);
            break;
        case Choice::Cancel:
            m_Pending = Action::None;
            break;
        }
    }
}
