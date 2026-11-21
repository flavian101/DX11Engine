#include "dxpch.h"
#include "AnimationSystem.h"
#include "Scene.h"
#include <algorithm>

namespace DXEngine
{
    namespace AnimationSystem
    {
        namespace
        {
            bool ValidClip(const AnimatorComponent& a) { return a.clip >= 0 && static_cast<size_t>(a.clip) < a.clips.size() && a.clips[a.clip]; }

            void Step(AnimatorComponent& a, float deltaSeconds)
            {
                AnimatorRuntime& rt = a.runtime;

                // the controller is a cache: (re)build it when the skeleton changed, or there is none yet
                if (!rt.controller || rt.controller->GetSkeleton() != a.skeleton)
                {
                    rt.controller = a.skeleton ? std::make_shared<AnimationController>(a.skeleton) : nullptr;
                    rt.clipIndex = -1;
                }
                if (!rt.controller) return;

                // The controller plays a clip of its own: give it ours. A different clip restarts it; a controller that is new
                // (first frame, or a copy of an entity) takes up at the component's time, so a duplicate shows the same pose.
                if (rt.clipIndex != a.clip || rt.restart)
                {
                    const bool restart = rt.restart || rt.clipIndex != -1;
                    rt.controller->SetClip(ValidClip(a) ? a.clips[a.clip] : nullptr);       // (resets the controller's time)
                    rt.clipIndex = a.clip;
                    rt.restart = false;
                    a.time = restart ? 0.0f : a.time;
                    rt.controller->SetTime(a.time);
                    rt.lastTime = a.time;
                }

                rt.controller->SetPlaybackSpeed(a.speed);
                rt.controller->SetPlaybackMode(a.mode);
                if (a.time != rt.lastTime)                   // somebody moved the time: follow it
                    rt.controller->SetTime(a.time);
                if (a.playing) rt.controller->Play(); else rt.controller->Pause();

                rt.controller->Update(deltaSeconds);

                a.time = rt.controller->GetCurrentTime();
                rt.lastTime = a.time;
                a.playing = a.playing && rt.controller->IsPlaying();   // a 'Once' clip that ended stops by itself

                if (ValidClip(a))
                    a.boneMatrices = rt.controller->GetBoneMatrices();
                else
                    a.boneMatrices.clear();                  // nothing to play: the skinned meshes draw unskinned
            }
        }

        void Update(Scene& scene, float deltaSeconds)
        {
            for (auto [entity, animator] : scene.Registry().view<AnimatorComponent>().each())
                Step(animator, deltaSeconds);
        }

        void SetClip(AnimatorComponent& animator, int clip)
        {
            if (animator.clips.empty()) { animator.clip = -1; return; }
            animator.clip = std::clamp(clip, 0, static_cast<int>(animator.clips.size()) - 1);
            animator.runtime.restart = true;                 // the next Update restarts it
        }

        void Play(AnimatorComponent& animator)
        {
            animator.playing = true;
            if (animator.mode == PlaybackMode::Once && ValidClip(animator))
            {
                const float duration = animator.clips[animator.clip]->GetDurationSeconds();
                if (animator.speed >= 0.0f && animator.time >= duration) animator.time = 0.0f;   // ended: start over
                if (animator.speed <  0.0f && animator.time <= 0.0f)     animator.time = duration;
            }
        }

        void Pause(AnimatorComponent& animator) { animator.playing = false; }

        void Stop(AnimatorComponent& animator)
        {
            animator.playing = false;
            animator.time = 0.0f;
        }

        const AnimatorComponent* FindAnimator(Scene& scene, entt::entity entity)
        {
            entt::registry& reg = scene.Registry();
            for (int depth = 0; entity != entt::null && reg.valid(entity) && depth < 4096; ++depth)   // (a cycle cannot exist, the bound is a belt)
            {
                if (const AnimatorComponent* animator = reg.try_get<AnimatorComponent>(entity))
                    return animator;
                const RelationshipComponent* rel = reg.try_get<RelationshipComponent>(entity);
                entity = rel ? rel->parent : entt::null;
            }
            return nullptr;
        }
    }
}
