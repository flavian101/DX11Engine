#pragma once
// ============================================================================
// Components: PLAIN DATA attached to entities. No behaviour that needs other objects,
// no inheritance, no "feature flags". A thing "is" what components it has:
//
//   a visible object   = Transform + MeshRenderer
//   a lamp             = Transform + Light            (+ a MeshRenderer if it should be visible)
//   a skinned character= Transform + MeshRenderer + (later) Skinned
//
// This replaces the Model class, which mixed all of this in one object using flags
// plus optional pointers that could disagree (the crash in Model::Update).
// ============================================================================
#include <DirectXMath.h>
#include <cmath>
#include <entt/entt.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace DXEngine
{
    class Mesh;       // only forward-declared: the scene core has no GPU dependency
    class Material;

    struct NameComponent
    {
        std::string name = "Entity";
    };

    // The parent entity. Absent = a root. Set through Scene::SetParent (it rejects cycles).
    struct ParentComponent
    {
        entt::entity parent = entt::null;
    };

    // ------------------------------------------------------------------
    // Transform: local position / rotation / scale. 'world' is DERIVED: it is written by
    // TransformSystem::Update once per frame (parents before children) and only read elsewhere.
    // ------------------------------------------------------------------
    struct TransformComponent
    {
        DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 rotation{ 0.0f, 0.0f, 0.0f, 1.0f };   // quaternion x, y, z, w
        DirectX::XMFLOAT3 scale{ 1.0f, 1.0f, 1.0f };

        DirectX::XMFLOAT4X4 world = Identity();                 // written by TransformSystem

        void SetEulerDegrees(float pitchX, float yawY, float rollZ)
        {
            using namespace DirectX;
            XMStoreFloat4(&rotation, XMQuaternionRotationRollPitchYaw(
                XMConvertToRadians(pitchX), XMConvertToRadians(yawY), XMConvertToRadians(rollZ)));
        }

        // scale, then rotate, then translate (DirectXMath is row-vector: S * R * T)
        DirectX::XMMATRIX Local() const
        {
            using namespace DirectX;
            return XMMatrixScaling(scale.x, scale.y, scale.z) *
                   XMMatrixRotationQuaternion(XMLoadFloat4(&rotation)) *
                   XMMatrixTranslation(position.x, position.y, position.z);
        }

        DirectX::XMMATRIX World() const { return DirectX::XMLoadFloat4x4(&world); }
        DirectX::XMFLOAT3 WorldPosition() const { return { world._41, world._42, world._43 }; }

        // the entity's +Z axis in world space, normalised (valid after TransformSystem::Update)
        DirectX::XMFLOAT3 WorldForward() const
        {
            const float x = world._31, y = world._32, z = world._33;
            const float len = std::sqrt(x * x + y * y + z * z);
            return len > 0.0f ? DirectX::XMFLOAT3{ x / len, y / len, z / len } : DirectX::XMFLOAT3{ 0, 0, 1 };
        }

        // Rotate so the entity's +Z axis points along 'direction' (used to aim directional and spot lights).
        // The up reference is world +Y; a (nearly) vertical direction falls back to +Z as the reference.
        void SetForward(const DirectX::XMFLOAT3& direction)
        {
            using namespace DirectX;
            const XMVECTOR raw = XMLoadFloat3(&direction);
            if (XMVectorGetX(XMVector3LengthSq(raw)) < 1e-8f) return;   // no direction: keep the current rotation
            XMVECTOR forward = XMVector3Normalize(raw);
            XMVECTOR up = XMVectorSet(0, 1, 0, 0);
            if (std::fabs(XMVectorGetY(forward)) > 0.9999f) up = XMVectorSet(0, 0, 1, 0);
            XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forward));   // left-handed: right = up x forward
            XMVECTOR realUp = XMVector3Cross(forward, right);
            XMMATRIX basis(right, realUp, forward, XMVectorSet(0, 0, 0, 1));
            XMStoreFloat4(&rotation, XMQuaternionNormalize(XMQuaternionRotationMatrix(basis)));
        }

        static DirectX::XMFLOAT4X4 Identity()
        {
            DirectX::XMFLOAT4X4 m;
            DirectX::XMStoreFloat4x4(&m, DirectX::XMMatrixIdentity());
            return m;
        }
    };

    // ------------------------------------------------------------------
    // MeshRenderer: draw this mesh. Assets (mesh, materials) are SHARED between entities; nothing
    // here is per-instance GPU state.
    // ------------------------------------------------------------------
    struct MeshRendererComponent
    {
        std::shared_ptr<Mesh> mesh;
        // One material per submesh. Missing entries fall back to the mesh's own material.
        std::vector<std::shared_ptr<Material>> materials;

        bool visible = true;
        bool castsShadows = true;
        bool receivesShadows = true;
    };

    // ------------------------------------------------------------------
    // Skinned meshes (bridge): where this mesh's bone matrices come from. The function is asked EVERY frame and
    // may return nullptr (no bones yet), so a missing skeleton is a normal case instead of a null read.
    // Transitional: the model importer still produces a Model that owns the skeleton and the animation
    // controller; this lets the scene draw it until the loader emits proper assets.
    // ------------------------------------------------------------------
    struct SkinnedMeshComponent
    {
        std::function<const std::vector<DirectX::XMFLOAT4X4>*()> boneMatrices;
    };

    // Per-frame update hook for things the scene does not own yet (the same bridge: advances a Model's animation).
    struct AnimationUpdateComponent
    {
        std::function<void(float deltaSeconds)> update;
    };

    // The sky: drawn first, follows the camera, never culled.
    struct SkyboxComponent {};

    // ------------------------------------------------------------------
    // Light: a light source. Position and direction come from the entity's Transform
    // (a directional/spot light points along its +Z axis).
    // ------------------------------------------------------------------
    enum class LightType : uint8_t { Directional, Point, Spot };

    struct LightComponent
    {
        LightType type = LightType::Point;
        DirectX::XMFLOAT3 color{ 1.0f, 1.0f, 1.0f };
        float intensity = 1.0f;
        float range = 10.0f;               // point/spot: distance where the light has fully faded
        float innerConeDegrees = 20.0f;    // spot only
        float outerConeDegrees = 30.0f;    // spot only
        bool  castsShadows = false;
    };
}
