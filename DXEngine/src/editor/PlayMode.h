#pragma once
#include "EditorContext.h"
#include "scene/GameRuntime.h"
#include "scene/SceneSnapshot.h"
#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>

namespace DXEngine
{
    // ==================================================================
    // PlayMode: what Play and Stop do to the scene, without any UI (so it is tested without a GPU or ImGui).
    //
    //   Start   the scene is written into memory (SceneSnapshot), the editor camera's pose is noted, the game starts.
    //           Refused, with nothing changed, when the scene cannot be written back completely.
    //   Stop    the game stops, the scene gets its old contents back (every component, every id), the selection and the
    //           camera are where they were. Whatever the game did to the scene is gone; that is what a snapshot is for.
    //
    // A play session cannot be left half done: Stop always ends it, even when the scene could not be restored (the outcome says
    // so, and the scene then keeps what the game made of it).
    //
    // Entity handles die with the restore. The whole selection is kept by id, and 'keep' on Start is for the host's own: the
    // restored entities come back in Outcome::kept, in the same order (invalid where an entity is gone).
    // ==================================================================
    class PlayMode
    {
    public:
        struct Outcome
        {
            bool ok = false;                          // Start: playing. Stop: the scene was restored.
            std::string error;                        // why not (empty when ok)
            std::vector<std::string> warnings;        // what could not be written (Start) or loaded back (Stop)
            std::vector<Entity> kept;                 // Stop: the entities passed as 'keep' on Start, found again
        };

        // 'game' may be null (a play session without gameplay: animations run, the camera flies). 'bridge' names and finds assets;
        // the same kind that scene files use, and one per call (it caches what it loads).
        Outcome Start(EditorContext& ctx, GameRuntime* game, std::unique_ptr<IAssetBridge> bridge, const std::vector<Entity>& keep = {});
        Outcome Stop(EditorContext& ctx, GameRuntime* game, std::unique_ptr<IAssetBridge> bridge);

        bool IsPlaying() const { return m_Playing; }

    private:
        Outcome Restore(EditorContext& ctx, IAssetBridge& bridge);

        SceneSnapshot m_Snapshot;
        std::vector<Entity> m_Kept;                   // the entities passed as 'keep' (their pins follow the selection's)
        size_t m_SelectedCount = 0;                   // how many entities were selected (they are pinned first, the primary last)
        bool m_Playing = false;

        // the editor camera is not part of the scene: its pose is put back by hand
        bool m_HasCamera = false;
        DirectX::XMFLOAT3 m_CameraPosition{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 m_CameraRotation{ 0.0f, 0.0f, 0.0f };
    };
}
