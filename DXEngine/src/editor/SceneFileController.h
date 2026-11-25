#pragma once
#include "EditorContext.h"
#include "scene/SceneSerializer.h"
#include <functional>
#include <memory>
#include <string>

namespace DXEngine
{
    // ==================================================================
    // SceneFileController: the editor's New / Open / Save / Save As, and the "unsaved changes" question that goes with them.
    //
    // It owns no UI window and no file dialog: the dialogs, the asset bridge and the log are handed in (Hooks), so all of it is
    // tested with a fake bridge, temp files and stand-in dialogs. The editor gives it the real ones.
    //
    // "Modified" is not tracked edit by edit. It is decided when it matters (New, Open): the scene is serialized and compared
    // with the text of the last save or load. That costs a serialization (milliseconds for thousands of entities) per command,
    // nothing per frame, and cannot miss an edit made by any panel. A scene that was never saved (or built in code, like the
    // editor's starting scene) counts as modified.
    // ==================================================================
    class SceneFileController
    {
    public:
        struct Hooks
        {
            std::function<std::string()> pickOpenPath;                          // "" = the user cancelled
            std::function<std::string(const std::string& suggested)> pickSavePath;
            std::function<std::unique_ptr<IAssetBridge>()> makeBridge;
            std::function<void(const std::string& line, bool warning)> log;     // the Assets panel's log
            // The scene's file changed: saved to a path, opened from one ("" = New: no file). The editor remembers it as the
            // scene to start with next time. Optional.
            std::function<void(const std::string& path)> pathChanged;
        };

        explicit SceneFileController(Hooks hooks) : m_Hooks(std::move(hooks)) {}

        // The commands. New / Open ask first when the scene has unsaved changes (the answer arrives through OnImGui).
        void New(EditorContext& ctx);
        void Open(EditorContext& ctx);

        // Opens a file without a dialog and without asking about unsaved changes: for the start-up, where the scene is the one built
        // in code and there is nothing to lose. False when it could not be opened (the log says why); the scene is then untouched.
        bool OpenFile(EditorContext& ctx, const std::string& path);
        bool Save(EditorContext& ctx);       // no file yet: Save As. False when it did not save (cancelled, failed)
        bool SaveAs(EditorContext& ctx);

        // Draws the "unsaved changes" modal while one is pending. Call every frame, anywhere inside the editor's ImGui frame.
        void OnImGui(EditorContext& ctx);

        // The three answers to that question (the modal's buttons call this; tests call it directly). Nothing pending: nothing happens.
        enum class Choice { Save, DontSave, Cancel };
        void AnswerPending(EditorContext& ctx, Choice choice);

        bool IsModified(EditorContext& ctx);
        const std::string& Path() const { return m_Path; }          // "" = never saved
        bool HasPending() const { return m_Pending != Action::None; }

    private:
        enum class Action { None, New, Open };

        void Proceed(EditorContext& ctx, Action action);
        bool WriteTo(EditorContext& ctx, const std::string& path);
        bool ReadFrom(EditorContext& ctx, const std::string& path);
        void RememberBaseline(EditorContext& ctx);
        void PathChanged() const { if (m_Hooks.pathChanged) m_Hooks.pathChanged(m_Path); }
        void Report(const std::string& line, bool warning) const { if (m_Hooks.log) m_Hooks.log(line, warning); }
        std::string CurrentText(EditorContext& ctx);

        Hooks m_Hooks;
        std::string m_Path;
        std::string m_Baseline;              // the text of the last save / load; empty = never
        bool m_HasBaseline = false;
        Action m_Pending = Action::None;
        bool m_OpenPopup = false;
    };
}
