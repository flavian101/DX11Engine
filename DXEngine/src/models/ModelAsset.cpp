#include "dxpch.h"
#include "ModelAsset.h"
#include "utils/Mesh/Mesh.h"
#include "utils/Mesh/Resource/MeshResource.h"
#include "utils/material/Material.h"

using namespace DirectX;

namespace DXEngine
{
    bool MeshHasSkinning(const Mesh& mesh)
    {
        const auto& resource = mesh.GetResource();
        if (!resource || !resource->GetVertexData()) return false;
        const VertexLayout& layout = resource->GetVertexData()->GetLayout();
        return layout.HasAttribute(RHI::VertexAttributeType::BlendIndices) && layout.HasAttribute(RHI::VertexAttributeType::BlendWeights);
    }

    uint32_t ModelAsset::AddNode(const std::string& nodeName, int parent, const XMFLOAT4X4& local)
    {
        ModelNode node;
        node.name = nodeName;
        node.parent = parent;

        XMVECTOR s, r, t;
        const bool split = XMMatrixDecompose(&s, &r, &t, XMLoadFloat4x4(&local)) &&
                           !XMVector3IsNaN(s) && !XMVector3IsInfinite(s) && !XMVector4IsNaN(r) && !XMVector4IsInfinite(r) &&
                           !XMVector3IsNaN(t) && !XMVector3IsInfinite(t);
        if (split)
        {
            XMStoreFloat3(&node.scale, s);
            XMStoreFloat4(&node.rotation, XMQuaternionNormalize(r));
            XMStoreFloat3(&node.position, t);
        }
        else
        {
            warnings.push_back("Node '" + nodeName + "' has a transform that cannot be split into position / rotation / scale; identity is used");
        }
        if (node.scale.x == 0.0f || node.scale.y == 0.0f || node.scale.z == 0.0f)
            warnings.push_back("Node '" + nodeName + "' has a zero scale: everything on it is invisible");
        return AddNode(node);
    }

    uint32_t ModelAsset::AddNode(const ModelNode& node)
    {
        nodes.push_back(node);
        return static_cast<uint32_t>(nodes.size() - 1);
    }

    uint32_t ModelAsset::AddMesh(const std::string& meshName, std::shared_ptr<Mesh> mesh)
    {
        ModelMesh entry;
        entry.name = meshName;
        entry.skinned = mesh && MeshHasSkinning(*mesh);
        entry.mesh = std::move(mesh);
        meshes.push_back(std::move(entry));
        return static_cast<uint32_t>(meshes.size() - 1);
    }

    void ModelAsset::AttachMesh(uint32_t node, uint32_t mesh)
    {
        if (node < nodes.size())
            nodes[node].meshes.push_back(mesh);
    }

    void ModelAsset::ApplyScale(float factor)
    {
        if (nodes.empty() || factor == 1.0f) return;
        ModelNode& root = nodes[0];
        root.scale = { root.scale.x * factor, root.scale.y * factor, root.scale.z * factor };
        root.position = { root.position.x * factor, root.position.y * factor, root.position.z * factor };
    }

    size_t ModelAsset::GetTotalVertexCount() const
    {
        size_t total = 0;
        for (const ModelMesh& m : meshes)
            if (m.mesh && m.mesh->GetResource() && m.mesh->GetResource()->GetVertexData())
                total += m.mesh->GetResource()->GetVertexData()->GetVertexCount();
        return total;
    }

    size_t ModelAsset::GetTotalTriangleCount() const
    {
        size_t total = 0;
        for (const ModelMesh& m : meshes)
            if (m.mesh && m.mesh->GetResource() && m.mesh->GetResource()->GetIndexData())
                total += m.mesh->GetResource()->GetIndexData()->GetIndexCount() / 3;
        return total;
    }

    size_t ModelAsset::GetMemoryUsage() const
    {
        size_t total = sizeof(*this);
        for (const ModelMesh& m : meshes)
            if (m.mesh && m.mesh->GetResource())
                total += m.mesh->GetResource()->GetMemoryUsage();
        return total;
    }

    std::vector<std::string> ModelAsset::Validate() const
    {
        std::vector<std::string> problems;

        if (nodes.empty())
            problems.push_back("The model has no nodes");
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            const ModelNode& n = nodes[i];
            if (i == 0 ? n.parent != -1 : (n.parent < 0 || static_cast<size_t>(n.parent) >= i))
                problems.push_back("Node " + std::to_string(i) + " ('" + n.name + "') has parent " + std::to_string(n.parent) + ": a parent must come before its children");
            for (uint32_t m : n.meshes)
                if (m >= meshes.size())
                    problems.push_back("Node " + std::to_string(i) + " ('" + n.name + "') uses mesh " + std::to_string(m) + " which does not exist");
        }

        bool anySkinned = false;
        for (size_t i = 0; i < meshes.size(); ++i)
        {
            if (!meshes[i].mesh || !meshes[i].mesh->IsValid())
                problems.push_back("Mesh " + std::to_string(i) + " ('" + meshes[i].name + "') is empty or invalid");
            anySkinned |= meshes[i].skinned;
        }
        if (anySkinned && !IsSkinned())
            problems.push_back("The model has skinned meshes but no skeleton");

        for (size_t i = 0; i < clips.size(); ++i)
            if (!clips[i])
                problems.push_back("Animation clip " + std::to_string(i) + " is null");
        return problems;
    }
}
