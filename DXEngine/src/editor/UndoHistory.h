#pragma once
#include "EditorContext.h"
#include "scene/SceneSnapshot.h"
#include <string>
#include <vector>

namespace DXEngine
{
    // ==================================================================
    // UndoHistory: the scene's states, one per finished edit, kept as text (SceneSerializer), with a position in them.
    // Undo and Redo move the position and put that state back, exactly as Stop puts back a play session's scene.
    //
    //     history.Reset(scene, bridge);          // the scene as it is now is step 0
    //     ... the user edits ...
    //     history.Record(scene, bridge);         // an edit is finished: the scene as it is now is a new step (no-op when unchanged)
    //     history.Undo(ctx, bridge, keep);       // the step before; Redo: the step after
    //
    // Why snapshots and not commands: every way of changing a scene (a property, a gizmo, a menu, a drag in the hierarchy, code
    // that a game system runs) is undone the same way and nothing has to be written twice for it. The price is the size of the
    // text, so the history is limited (steps and bytes: the oldest go first) and a step is recorded when a gesture ENDS (a drag
    // is one step), never per frame.
    //
    // Like a play session's snapshot, a state is recorded only when the scene can be written back COMPLETELY: otherwise an undo
    // would silently drop an attachment (a mesh made by code, with no name a file could use). Then the history is cleared and
    // says why; the next Record or Reset starts a new one when the scene can be written again.
    //
    // Assets are not part of a step (principle 3): a mesh, a texture or an imported model's own material is found again by name,
    // so a change made to one of those is not undone. A scene's own materials are.
    //
    // Entity handles die with every Undo / Redo. The whole selection is kept by id (the entities that are not in that state drop out of it), and 'keep' is for the host's own (expanded rows,
    // the stress-test parent): they come back in Outcome::kept in the same order, invalid where the entity is not in that state.
    // ==================================================================
    class UndoHistory
    {
    public:
        struct Limits
        {
            size_t maxSteps = 128;                      // states kept, the current one and the redo ones included
            size_t maxBytes = 96u * 1024u * 1024u;      // their text together (the newest state is always kept)
        };

        struct Outcome
        {
            bool ok = false;                            // Reset / Record: the history is active. Undo / Redo: the scene was put back.
            bool changed = false;                       // Record: a new step was added
            std::string error;
            std::vector<std::string> warnings;          // what could not be written (Reset / Record) or loaded back (Undo / Redo)
            std::vector<Entity> kept;                   // Undo / Redo: 'keep', found again
        };

        UndoHistory() = default;
        explicit UndoHistory(Limits limits) : m_Limits(limits) {}

        // The scene as it is now becomes the only step. Not ok (and no history) when it cannot be written back completely.
        Outcome Reset(Scene& scene, IAssetBridge& bridge);

        // The scene as it is now becomes the newest step when it differs from the current one; steps that were undone are dropped.
        // Not ok when the scene cannot be written back completely: the history is cleared (see above). Does nothing without a history.
        Outcome Record(Scene& scene, IAssetBridge& bridge);

        // The history before / after the current step; the scene gets that state. Nothing to undo / redo: not ok, nothing changes.
        Outcome Undo(EditorContext& ctx, IAssetBridge& bridge, const std::vector<Entity>& keep = {});
        Outcome Redo(EditorContext& ctx, IAssetBridge& bridge, const std::vector<Entity>& keep = {});

        bool IsActive() const { return !m_States.empty(); }
        bool CanUndo() const { return m_Index > 0; }
        bool CanRedo() const { return IsActive() && m_Index + 1 < m_States.size(); }
        size_t Steps() const { return m_States.size(); }
        size_t Position() const { return m_Index; }          // 0 = the oldest state kept
        size_t Bytes() const { return m_Bytes; }
        void Clear() { m_States.clear(); m_Index = 0; m_Bytes = 0; }

    private:
        Outcome Step(EditorContext& ctx, IAssetBridge& bridge, const std::vector<Entity>& keep, size_t target);
        void Trim();

        Limits m_Limits;
        std::vector<std::string> m_States;
        size_t m_Index = 0;
        size_t m_Bytes = 0;
    };
}
