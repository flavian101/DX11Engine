#pragma once
#include "Components.h"
#include <entt/entt.hpp>

namespace DXEngine
{
    class Scene;

    // Advances everything that animates. Run once per frame with the frame's delta in SECONDS, before
    // TransformSystem::Update.
    namespace AnimationSystem
    {
        // For every Animator: builds its controller if it has none, applies its settings (clip, speed, mode, playing, a
        // scrubbed time), steps it by 'deltaSeconds' and stores the new time, state and bone matrices back in the component.
        void Update(Scene& scene, float deltaSeconds);

        // Settings edits (what the inspector's buttons do). They only change the component; Update applies them.
        void SetClip(AnimatorComponent& animator, int clip);     // out of range is clamped; restarts the clip
        void Play(AnimatorComponent& animator);                  // from the end of a 'Once' clip it starts over
        void Pause(AnimatorComponent& animator);
        void Stop(AnimatorComponent& animator);                  // pause and back to the start

        // The Animator a skinned mesh draws with: the nearest one on the entity itself or above it, else nullptr.
        const AnimatorComponent* FindAnimator(Scene& scene, entt::entity entity);

        // The entity that holds that Animator (entt::null when there is none). A skinned mesh is drawn in ITS space: the
        // bone matrices map the mesh into the skeleton's root space, and the root entity's world matrix places that space
        // in the scene. (The mesh entity's own transform is not part of the pose; it is ignored, as glTF specifies.)
        entt::entity FindAnimatorEntity(Scene& scene, entt::entity entity);
    }
}
