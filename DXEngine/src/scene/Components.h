#pragma once
// ============================================================================
// Components: PLAIN DATA attached to entities. No behaviour that needs other objects,
// no inheritance, no "feature flags". A thing "is" what components it has:
//
//   a visible object   = Transform + MeshRenderer
//   a lamp             = Transform + Light            (+ a MeshRenderer if it should be visible)
//   a skinned character= Transform + MeshRenderer + Skinned, under an entity with an Animator
//
// This replaces the Model class, which mixed all of this in one object using flags
// plus optional pointers that could disagree (the crash in Model::Update).
// ============================================================================
#include "Animation/AnimationController.h"
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

    // Where an entity sits in the hierarchy: its parent and its children, as links between entities.
    //
    // The children of an entity form a doubly linked list (firstChild .. lastChild, joined by prevSibling / nextSibling),
    // in the order they were attached. That makes "the children of X", "detach X" and "the subtree of X" cost what they
    // touch and never a scan of all entities. (prevSibling is there so detaching from the middle of a list is O(1).)
    //
    // Absent = a root without children. ONLY Scene writes this component (SetParent, DestroyEntity); everything else
    // reads it, normally through Scene::GetParent / GetChildren / FirstChild / NextSibling. Scene keeps it consistent:
    // it is added when an entity gets a parent or a child and removed again when both are gone.
    struct RelationshipComponent
    {
        entt::entity parent      = entt::null;   // null = a root
        entt::entity firstChild  = entt::null;   // null = no children
        entt::entity lastChild   = entt::null;
        entt::entity prevSibling = entt::null;   // within the parent's child list
        entt::entity nextSibling = entt::null;
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
    // Animator: plays the animation clips of one skeleton and holds the bone matrices the skinned meshes UNDER it draw
    // with. It sits on the root of an instantiated model; AnimationSystem advances it once per frame.
    //
    // The skeleton and the clips are asset data (shared by every instance). The settings (clip, speed, mode, playing, time)
    // are plain values the inspector edits; the system reads them every frame, so changing one is all it takes. The
    // controller that does the stepping is a cache the system builds when it is missing: copying an Animator (duplicating
    // an entity) gives the copy its own controller, never a shared one.
    // ------------------------------------------------------------------
    struct AnimatorRuntime
    {
        std::shared_ptr<AnimationController> controller;
        int   clipIndex = -1;        // the clip the controller was set to (-1 = not set yet)
        float lastTime  = 0.0f;      // the time the system wrote last: a different 'time' means someone scrubbed
        bool  restart   = false;     // SetClip asked for the clip to start from 0

        AnimatorRuntime() = default;
        AnimatorRuntime(const AnimatorRuntime&) {}                                  // a copy starts without a controller
        AnimatorRuntime& operator=(const AnimatorRuntime&) { controller.reset(); clipIndex = -1; lastTime = 0.0f; restart = false; return *this; }
    };

    struct AnimatorComponent
    {
        std::shared_ptr<Skeleton>                   skeleton;
        std::vector<std::shared_ptr<AnimationClip>> clips;

        int          clip    = 0;                    // index into 'clips'; out of range = nothing plays
        float        speed   = 1.0f;                 // 1 = as authored, negative plays backwards
        PlaybackMode mode    = PlaybackMode::Loop;
        bool         playing = true;
        float        time    = 0.0f;                 // seconds into the clip; the system keeps it current, writing it scrubs

        std::vector<DirectX::XMFLOAT4X4> boneMatrices;   // output: one matrix per bone, empty until the first update

        AnimatorRuntime runtime;
    };

    // A mesh drawn with bones: it uses the bone matrices of the nearest Animator among its ancestors (found when it is
    // drawn, so reparenting or duplicating a model never leaves a dangling reference). No Animator above = drawn unskinned.
    struct SkinnedMeshComponent {};

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
