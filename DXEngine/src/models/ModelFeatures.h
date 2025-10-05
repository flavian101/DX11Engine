#pragma once
#include <cstdint>
#include <DirectXMath.h>
#include <vector>
#include <memory>
#include "Animation/AnimationController.h"

namespace DXEngine
{
	class AnimationController;

	//model Capabilities
	enum class ModelFeature : uint32_t
	{
        None = 0,
        Static = 1 << 0,        // Default static mesh
        Instanced = 1 << 1,      // Can be instanced
        Skinned = 1 << 2,        // Has skeletal animation
        Morph = 1 << 3,          // Has morph targets
        LOD = 1 << 4,            // Has LOD levels
        DetailMapped = 1 << 5,   // Uses detail textures
        Tessellated = 1 << 6,    // Uses tessellation

        // Common combinations
        InstancedStatic = Static | Instanced,
        SkinnedAnimated = Skinned,
        All = 0xFFFFFFFF
	};
    
    //Bitwise operators for Feature flags
    inline ModelFeature operator|(ModelFeature a, ModelFeature b)
    {
        return static_cast<ModelFeature>(
            static_cast<uint32_t>(a) | static_cast<uint32_t>(b)
            );
    }
    inline ModelFeature operator&(ModelFeature a, ModelFeature b)
    {
        return static_cast<ModelFeature>(
            static_cast<uint32_t>(a) & static_cast<uint32_t>(b)
            );
    }
    inline bool HasFeature(ModelFeature flags, ModelFeature feature)
    {
        return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(feature)) != 0;
    }

    // ====== Feature Data Components ======

    // Component for instancing data
    struct InstanceData
    {
        std::vector<DirectX::XMFLOAT4X4> transforms;
        bool dirty = false;

        size_t GetInstanceCount()const { return transforms.size(); }

        void AddInstance(const DirectX::XMFLOAT4X4& transform)
        {
            transforms.push_back(transform);
            dirty = true;
        }
        void SetInstances(const std::vector<DirectX::XMFLOAT4X4>& newTransfoms)
        {
            transforms = newTransfoms;
            dirty = true;
        }
        void ClearInstances()
        {
            transforms.clear();
            dirty = true;
        }
    };

    struct SkinningData
    {
        std::vector<DirectX::XMFLOAT4X4> boneMatrices;
        std::shared_ptr<Skeleton> skeleton;
        std::shared_ptr<AnimationController> animationController;
        std::vector<std::shared_ptr<AnimationClip>> animationClips;
        std::unordered_map<std::string, size_t> animationClipNameMap;
        bool dirty = true;

        void UpdateBoneMatrices(const std::vector<DirectX::XMFLOAT4X4>& matrices)
        {
            boneMatrices = matrices;
            dirty = true;
        }
        void AddAnimationClip(std::shared_ptr<AnimationClip> clip)
        {
            if (clip)
                return;
            AddAnimationClip(clip->GetName(), clip);
            animationClips.push_back(clip);
        }
        void AddAnimationClip(const std::string& name, std::shared_ptr<AnimationClip> clip)
        {
            if (clip)
            {
                return;
            }
            //check if with this name already exists
            auto it = animationClipNameMap.find(name);
            if (it != animationClipNameMap.end())
            {
                //replace existing clip
                animationClips[it->second] = clip;
                OutputDebugStringA(("Warning: Replacing existing animation clip: " + name + "\n").c_str());
            }
            else
            {
                //add new clip
                size_t index = animationClips.size();
                animationClips.push_back(clip);
                animationClipNameMap[name] = index;
            }
        }

        std::shared_ptr<AnimationClip> GetAnimationClip(const std::string& name)
        {
            auto it = animationClipNameMap.find(name);
            if (it != animationClipNameMap.end())
            {
                return animationClips[it->second];
            }
            return nullptr;
        }
        std::shared_ptr<AnimationClip> GetAnimationClip(size_t index) const
        {
            if (index >= animationClips.size())
            {
                return nullptr;
            }
            return animationClips[index];
        }

        std::vector<std::string> GetAnimationClipNames() const
        {
            std::vector<std::string> names;
            names.reserve(animationClips.size());

            for (const auto& clip : animationClips)
            {
                if (clip)
                {
                    names.push_back(clip->GetName());
                }
            }
            return names;
        }
        const std::vector<std::shared_ptr<AnimationClip>>& GetAllAnimationClips()const
        {
            return animationClips;
        }

        void PlayAnimation(std::shared_ptr<AnimationClip> clip, PlaybackMode mode)
        {
            if (!animationController && skeleton)
            {
                animationController = std::make_shared<AnimationController>(skeleton);
            }

            if (animationController)
            {
                animationController->SetClip(clip);
                animationController->SetPlaybackMode(mode);
                animationController->Play();
            }
        }

        void PlayAnimation(const std::string& name, PlaybackMode mode)
        {
            auto clip = GetAnimationClip(name);
            if (!clip)
            {
                OutputDebugStringA(("Warning: Animation clip not found: " + name + "\n").c_str());
                return;
            }

            PlayAnimation(clip, mode);
        }

        void PlayAnimation(size_t index, PlaybackMode mode)
        {
            auto clip = GetAnimationClip(index);
            if (!clip)
            {
                OutputDebugStringA(("Warning: Animation clip index out of range: " +
                    std::to_string(index) + "\n").c_str());
                return;
            }

            PlayAnimation(clip, mode);
        }

        void StopAnimation()
        {
            if (animationController)
            {
                animationController->Stop();
            }
        }

        void PauseAnimation()
        {
            if (animationController)
            {
                animationController->Pause();
            }
        }

        void ResumeAnimation()
        {
            if (animationController)
            {
                animationController->Play();
            }
        }

        bool HasValidSkeleton()const
        {
            return skeleton && skeleton->GetBoneCount() > 0;
        }
    };

    // Component for LOD data
    struct LODData
    {
        struct LODLevel
        {
            float distance;
            size_t meshIndex;
        };

        std::vector<LODLevel> levels;
        size_t currentLevel = 0;

        size_t SelectLOD(float distance) const
        {
            for (size_t i = 0; i < levels.size(); ++i)
            {
                if (distance < levels[i].distance)
                    return levels[i].meshIndex;
            }
            return levels.empty() ? 0 : levels.back().meshIndex;
        }
    };

    // Component for morph target data
    struct MorphData
    {
        struct MorphTarget
        {
            std::string name;
            std::vector<DirectX::XMFLOAT3> positionDeltas;
            std::vector<DirectX::XMFLOAT3> normalDeltas;
            float weight = 0.0f;
        };

        std::vector<MorphTarget> targets;
        bool dirty = true;
    };

}