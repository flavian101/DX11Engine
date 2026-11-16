#include "dxpch.h"
#include "SceneFactory.h"
#include "models/ModelAsset.h"
#include "utils/Mesh/Mesh.h"

namespace DXEngine
{
    namespace SceneFactory
    {
        Entity Instantiate(Scene& scene, const std::shared_ptr<ModelAsset>& asset, const std::string& name)
        {
            if (!asset || asset->nodes.empty()) return Entity();

            // one entity per node, in the asset's order (a parent always comes before its children)
            std::vector<Entity> entities(asset->nodes.size());
            for (size_t i = 0; i < asset->nodes.size(); ++i)
            {
                const ModelNode& node = asset->nodes[i];
                const bool hasParent = i > 0 && node.parent >= 0 && static_cast<size_t>(node.parent) < i;
                Entity parent = hasParent ? entities[node.parent] : (i > 0 ? entities[0] : Entity());   // a broken parent: under the root

                Entity entity = scene.CreateEntity(i == 0 ? name : node.name, parent);
                auto& t = entity.Get<TransformComponent>();
                t.position = node.position;
                t.rotation = node.rotation;
                t.scale    = node.scale;
                entities[i] = entity;
            }
            Entity root = entities[0];

            // the skeleton's animation: one Animator on the root, shared skeleton and clips, its own playback state
            if (asset->IsSkinned())
            {
                auto& animator = root.Add<AnimatorComponent>();
                animator.skeleton = asset->skeleton;
                animator.clips    = asset->clips;
                animator.clip     = asset->clips.empty() ? -1 : 0;
                animator.playing  = !asset->clips.empty();
            }

            // the meshes: a mesh on a node uses the node's entity when it is the only one there; several meshes on one
            // node (or a skinned mesh, whose vertices already are in model space) get an entity of their own
            for (size_t i = 0; i < asset->nodes.size(); ++i)
            {
                const ModelNode& node = asset->nodes[i];
                for (size_t k = 0; k < node.meshes.size(); ++k)
                {
                    if (node.meshes[k] >= asset->meshes.size()) continue;
                    const ModelMesh& entry = asset->meshes[node.meshes[k]];
                    if (!entry.mesh) continue;

                    const bool skinned = asset->IsSkinned() && entry.skinned;
                    Entity target;
                    if (!skinned && node.meshes.size() == 1)
                        target = entities[i];
                    else
                        target = scene.CreateEntity(entry.name.empty() ? node.name + "_mesh" + std::to_string(k) : entry.name,
                                                    skinned ? root : entities[i]);

                    // no material list: the mesh's own materials (assigned by the importer) are used
                    target.Add<MeshRendererComponent>().mesh = entry.mesh;
                    if (skinned)
                        target.Add<SkinnedMeshComponent>();
                }
            }
            return root;
        }
    }
}
