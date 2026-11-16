#pragma once
// ============================================================================
// ModelAsset: what the importer produces for one model file. IMMUTABLE DATA, shared by every instance: the node tree
// (names + local transforms + which meshes hang on which node), the meshes, their materials, the skeleton and the clips.
//
// It is not an object that lives in the world. Scene::Instantiate (SceneFactory::Instantiate) turns it into entities;
// from then on the scene owns the instance and the asset only supplies the shared data (meshes, materials, skeleton,
// clips). This replaces the Model class, which was both the import result and a thing with a position and an animation.
// ============================================================================
#include "Animation/AnimationClip.h"
#include <DirectXMath.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace DXEngine
{
    class Mesh;
    class Material;

    struct ModelNode
    {
        std::string name;
        int         parent = -1;                       // index of the parent node; -1 only for the root (nodes[0])
        DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };      // local transform
        DirectX::XMFLOAT4 rotation = { 0.0f, 0.0f, 0.0f, 1.0f };  // quaternion
        DirectX::XMFLOAT3 scale    = { 1.0f, 1.0f, 1.0f };
        std::vector<uint32_t> meshes;                  // indices into ModelAsset::meshes
    };

    struct ModelMesh
    {
        std::string           name;
        std::shared_ptr<Mesh> mesh;
        bool                  skinned = false;         // the vertices carry bone indices and weights
    };

    class ModelAsset
    {
    public:
        std::string name;                              // file name without folders
        std::string sourcePath;                        // where it was imported from

        std::vector<ModelNode> nodes;                  // nodes[0] is the root; a parent always comes before its children
        std::vector<ModelMesh> meshes;
        std::vector<std::shared_ptr<Material>>      materials;   // every material the meshes use, each once
        std::shared_ptr<Skeleton>                   skeleton;    // null when nothing in the file is skinned
        std::vector<std::shared_ptr<AnimationClip>> clips;

        // Problems found while importing that did not stop the import (a texture that could not be read, a clip without a
        // skeleton, ...). The editor shows them: an import that quietly drops things is worse than one that says so.
        std::vector<std::string> warnings;

        // ---- building (the importer, tests)
        // 'local' is the node's transform relative to its parent, in the engine's row-vector convention.
        uint32_t AddNode(const std::string& nodeName, int parent, const DirectX::XMFLOAT4X4& local);
        uint32_t AddNode(const ModelNode& node);
        uint32_t AddMesh(const std::string& meshName, std::shared_ptr<Mesh> mesh);   // 'skinned' is read from the vertex layout
        void     AttachMesh(uint32_t node, uint32_t mesh);

        // A uniform scale on the whole model (the root node carries it: every instance gets it).
        void ApplyScale(float factor);

        // ---- queries
        bool   IsSkinned() const { return skeleton && skeleton->GetBoneCount() > 0; }
        size_t GetTotalVertexCount() const;
        size_t GetTotalTriangleCount() const;
        size_t GetMemoryUsage() const;

        // Everything wrong with the structure, one line each (empty = fine): a node whose parent does not come first, a
        // mesh index that does not exist, a null mesh, skinned meshes without a skeleton, a clip that is null.
        std::vector<std::string> Validate() const;
    };

    // true when the mesh's vertices carry bone indices AND weights
    bool MeshHasSkinning(const Mesh& mesh);
}
