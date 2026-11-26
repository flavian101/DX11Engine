#include "dxpch.h"
#include "SceneSnapshot.h"
#include "TransformSystem.h"

namespace DXEngine
{
    SceneSnapshot::TakeResult SceneSnapshot::Take(Scene& scene, IAssetBridge& bridge)
    {
        TakeResult result;
        SceneSerializer::SaveResult saved = SceneSerializer::Save(scene, bridge);
        if (!saved.warnings.empty())
        {
            result.error = "the scene cannot be written back completely";
            result.warnings = std::move(saved.warnings);
            return result;                                  // the old snapshot, if any, is untouched
        }
        m_Text = std::move(saved.text);
        result.ok = true;
        return result;
    }

    SceneSnapshot::RestoreResult SceneSnapshot::Restore(Scene& scene, IAssetBridge& bridge)
    {
        return RestoreText(m_Text, scene, bridge);
    }

    SceneSnapshot::RestoreResult SceneSnapshot::RestoreText(const std::string& text, Scene& scene, IAssetBridge& bridge)
    {
        RestoreResult result;
        if (text.empty()) { result.error = "there is no snapshot to restore"; return result; }

        // The scene about to be replaced is still alive while the snapshot loads (Load builds the new one on the side), so the
        // bridge can hand out what that scene holds instead of reading the same files again.
        bridge.Reuse(scene);

        SceneSerializer::LoadResult loaded = SceneSerializer::Load(text, scene, bridge);
        if (!loaded.ok) { result.error = std::move(loaded.error); return result; }       // the scene is untouched

        TransformSystem::Update(scene);                     // world matrices are not saved
        result.ok = true;
        result.warnings = std::move(loaded.warnings);
        result.entities = loaded.entities;
        return result;
    }

    size_t SceneSnapshot::Pin(Scene& scene, Entity entity)
    {
        m_Pins.push_back(entity.IsValid() ? scene.GetUuid(entity) : Uuid());
        return m_Pins.size() - 1;
    }

    Entity SceneSnapshot::Pinned(Scene& scene, size_t index) const
    {
        if (index >= m_Pins.size() || m_Pins[index].IsNull()) return Entity();
        return scene.FindByUuid(m_Pins[index]);
    }
}
