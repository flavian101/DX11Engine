#include "dxpch.h"
#include "PlayMode.h"

namespace DXEngine
{
    PlayMode::Outcome PlayMode::Start(EditorContext& ctx, GameRuntime* game, std::unique_ptr<IAssetBridge> bridge, const std::vector<Entity>& keep)
    {
        Outcome out;
        if (m_Playing) { out.error = "the scene is already playing"; return out; }
        if (!ctx.scene) { out.error = "there is no scene to play"; return out; }
        if (!bridge) { out.error = "the scene cannot be saved for the play session (no asset bridge)"; return out; }

        SceneSnapshot::TakeResult taken = m_Snapshot.Take(*ctx.scene, *bridge);
        if (!taken.ok)
        {
            out.error = std::move(taken.error);
            out.warnings = std::move(taken.warnings);
            return out;                                     // nothing changed: the scene, the camera and the game are as they were
        }

        m_Snapshot.Pin(*ctx.scene, ctx.selection.Get());   // pin 0 is the selection
        for (const Entity& e : keep) m_Snapshot.Pin(*ctx.scene, e);
        m_Kept = keep;

        m_HasCamera = ctx.camera && ctx.camera->GetCamera();
        if (m_HasCamera)
        {
            Camera& camera = *ctx.camera->GetCamera();
            m_CameraPosition = camera.GetPositionFloat3();
            m_CameraRotation = camera.GetRotationFloat3();
        }

        m_Playing = true;
        if (game && !game->Start(*ctx.scene))
        {
            // a system failed to start: the game has stopped the ones that did, and whatever they changed is put back with the rest
            const std::string why = game->LastError();
            Outcome undone = Restore(ctx, *bridge);
            out.error = "the game could not start: " + why;
            out.warnings = std::move(undone.warnings);
            if (!undone.ok) out.error += " (and the scene could not be restored: " + undone.error + ")";
            return out;
        }

        out.ok = true;
        return out;
    }

    PlayMode::Outcome PlayMode::Stop(EditorContext& ctx, GameRuntime* game, std::unique_ptr<IAssetBridge> bridge)
    {
        if (!m_Playing)
        {
            Outcome out;
            out.error = "the scene is not playing";
            return out;
        }
        if (game && ctx.scene) game->Stop(*ctx.scene);      // (does nothing when a failing system already stopped it)
        if (!bridge)
        {
            Outcome out;
            m_Playing = false;                              // the session still ends; the scene keeps what the game made of it
            m_Snapshot.Clear();
            out.error = "no asset bridge to put the scene back with";
            return out;
        }
        return Restore(ctx, *bridge);
    }

    PlayMode::Outcome PlayMode::Restore(EditorContext& ctx, IAssetBridge& bridge)
    {
        Outcome out;
        m_Playing = false;

        SceneSnapshot::RestoreResult back = m_Snapshot.Restore(*ctx.scene, bridge);
        out.warnings = std::move(back.warnings);
        out.ok = back.ok;
        if (!back.ok) out.error = std::move(back.error);

        if (back.ok)
        {
            ctx.selection.Set(m_Snapshot.Pinned(*ctx.scene, 0));
            out.kept.reserve(m_Kept.size());
            for (size_t i = 0; i < m_Kept.size(); ++i) out.kept.push_back(m_Snapshot.Pinned(*ctx.scene, 1 + i));
        }
        else
            out.kept = m_Kept;                              // the scene was not replaced: the old handles are still the ones to use

        if (m_HasCamera && ctx.camera && ctx.camera->GetCamera())
        {
            Camera& camera = *ctx.camera->GetCamera();
            camera.SetPosition(m_CameraPosition);
            camera.SetRotation(m_CameraRotation);
        }
        m_Snapshot.Clear();
        m_Kept.clear();
        return out;
    }
}
