#include "dxpch.h"
#include "UndoHistory.h"

namespace DXEngine
{
    UndoHistory::Outcome UndoHistory::Reset(Scene& scene, IAssetBridge& bridge)
    {
        Clear();
        Outcome out;
        SceneSerializer::SaveResult saved = SceneSerializer::Save(scene, bridge);
        if (!saved.warnings.empty())
        {
            out.error = "the scene cannot be written back completely";
            out.warnings = std::move(saved.warnings);
            return out;
        }
        m_Bytes = saved.text.size();
        m_States.push_back(std::move(saved.text));
        m_Index = 0;
        out.ok = true;
        return out;
    }

    UndoHistory::Outcome UndoHistory::Record(Scene& scene, IAssetBridge& bridge)
    {
        Outcome out;
        if (!IsActive()) { out.error = "there is no history"; return out; }

        SceneSerializer::SaveResult saved = SceneSerializer::Save(scene, bridge);
        if (!saved.warnings.empty())
        {
            Clear();                                        // an undo could not bring everything back: better none than a lossy one
            out.error = "the scene cannot be written back completely: undo is off";
            out.warnings = std::move(saved.warnings);
            return out;
        }

        out.ok = true;
        if (saved.text == m_States[m_Index]) return out;     // nothing changed (a click that edited nothing)

        while (m_States.size() > m_Index + 1)               // what was undone can no longer be redone
        {
            m_Bytes -= m_States.back().size();
            m_States.pop_back();
        }
        m_Bytes += saved.text.size();
        m_States.push_back(std::move(saved.text));
        m_Index = m_States.size() - 1;
        Trim();
        out.changed = true;
        return out;
    }

    UndoHistory::Outcome UndoHistory::Undo(EditorContext& ctx, IAssetBridge& bridge, const std::vector<Entity>& keep)
    {
        if (!CanUndo()) { Outcome out; out.error = "nothing to undo"; return out; }
        return Step(ctx, bridge, keep, m_Index - 1);
    }

    UndoHistory::Outcome UndoHistory::Redo(EditorContext& ctx, IAssetBridge& bridge, const std::vector<Entity>& keep)
    {
        if (!CanRedo()) { Outcome out; out.error = "nothing to redo"; return out; }
        return Step(ctx, bridge, keep, m_Index + 1);
    }

    UndoHistory::Outcome UndoHistory::Step(EditorContext& ctx, IAssetBridge& bridge, const std::vector<Entity>& keep, size_t target)
    {
        Outcome out;
        if (!ctx.scene) { out.error = "there is no scene"; return out; }

        // Handles die with the restore: remember the selection and the host's entities by id.
        SceneSnapshot pins;
        pins.Pin(*ctx.scene, ctx.selection.Get());
        for (const Entity& e : keep) pins.Pin(*ctx.scene, e);

        SceneSnapshot::RestoreResult back = SceneSnapshot::RestoreText(m_States[target], *ctx.scene, bridge);
        out.warnings = std::move(back.warnings);
        if (!back.ok)
        {
            out.error = std::move(back.error);              // the scene is untouched and so is the position
            out.kept = keep;
            return out;
        }

        m_Index = target;
        ctx.selection.Set(pins.Pinned(*ctx.scene, 0));
        out.kept.reserve(keep.size());
        for (size_t i = 0; i < keep.size(); ++i) out.kept.push_back(pins.Pinned(*ctx.scene, 1 + i));
        out.ok = true;
        return out;
    }

    void UndoHistory::Trim()
    {
        // drop the oldest states while there are too many or too much text; the newest one always stays
        size_t drop = 0;
        size_t bytes = m_Bytes;
        while (m_States.size() - drop > 1 && (m_States.size() - drop > m_Limits.maxSteps || bytes > m_Limits.maxBytes))
        {
            bytes -= m_States[drop].size();
            ++drop;
        }
        if (drop == 0) return;
        m_States.erase(m_States.begin(), m_States.begin() + drop);
        m_Bytes = bytes;
        m_Index = m_Index >= drop ? m_Index - drop : 0;
    }
}
