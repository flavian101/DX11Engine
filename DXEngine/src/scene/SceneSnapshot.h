#pragma once
#include "scene/Scene.h"
#include "scene/SceneSerializer.h"
#include <string>
#include <vector>

namespace DXEngine
{
    // ==================================================================
    // SceneSnapshot: a scene kept in memory as text (SceneSerializer) so it can be put back later. Play mode is the user: the
    // scene is taken when Play starts and restored when it stops, whatever the game did to it in between.
    //
    //     SceneSnapshot snapshot;
    //     auto taken = snapshot.Take(scene, bridge);          // refused when something could not be written
    //     ... the scene changes ...
    //     auto back = snapshot.Restore(scene, bridge);        // the scene has its old contents again
    //
    // What comes back is the scene: entities, ids, hierarchy, components, the values of materials that belong to the scene.
    // It is not the ASSETS (principle 3: assets are shared and immutable): a mesh, a texture or an imported model's own
    // material is found again by its name, and is the object that is already loaded, so a change made to it in between stays.
    //
    // Refusing. Take succeeds only when the scene was written COMPLETELY. A warning from the serializer means "this
    // attachment has no name a file could use" (a mesh made by code, a texture that is not a file): restoring would drop it
    // and the user would lose it without having asked for anything. Better to say so before the game starts.
    //
    // Entity handles do not survive a restore: the scene's contents are replaced, and an old handle may be invalid or (EnTT hands
    // out the same numbers again) point at a different entity. Ids survive. Pin() remembers an entity by its id so the host can get
    // the right one back: the selection, an expanded row, anything it holds on to.
    // ==================================================================
    class SceneSnapshot
    {
    public:
        struct TakeResult
        {
            bool ok = false;
            std::string error;                      // why nothing was taken (empty when ok)
            std::vector<std::string> warnings;      // what could not be written; the reason for a refusal
        };

        struct RestoreResult
        {
            bool ok = false;
            std::string error;                      // why the scene was left as it was (empty when ok)
            std::vector<std::string> warnings;      // what was dropped or repaired while loading (a texture file that is gone)
            size_t entities = 0;
        };

        // Writes the scene into memory. Pins made earlier stay; a snapshot taken before is replaced only when this one succeeds.
        TakeResult Take(Scene& scene, IAssetBridge& bridge);

        // Replaces the scene's contents with the snapshot and brings the world matrices up to date. The snapshot stays (restoring
        // twice gives the same scene twice); Clear() lets it go. A scene that cannot be restored (no snapshot, text the serializer
        // refuses) is left exactly as it is.
        RestoreResult Restore(Scene& scene, IAssetBridge& bridge);

        bool IsEmpty() const { return m_Text.empty(); }
        const std::string& Text() const { return m_Text; }
        void Clear() { m_Text.clear(); m_Pins.clear(); }

        // Remembers an entity of 'scene' by its id and returns its index (an invalid entity is remembered as "nothing").
        size_t Pin(Scene& scene, Entity entity);
        // The pinned entity as it is in 'scene' now: invalid when nothing was pinned there, or the entity is gone (it was
        // created after the snapshot, say). Look it up after Restore: before it, the answer is the entity itself.
        Entity Pinned(Scene& scene, size_t index) const;
        size_t PinCount() const { return m_Pins.size(); }

    private:
        std::string m_Text;
        std::vector<Uuid> m_Pins;
    };
}
