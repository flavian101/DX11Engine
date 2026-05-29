#pragma once
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <any>
#include <DirectXCollision.h>
#include <DirectXMath.h>
#include "FrameTime.h"

namespace DXEngine::Rendering
{
	class Model;
	class Camera;
	class UIElement;

	namespace RHI
	{
		class IGraphicsDevice;
		class ICommandBuffer;
		class ITexture;
	}
	class CullingSystem;
	class RenderBatcher;
	class ShaderCache;

	/// <summary>
	/// Frame Context - Blackboard passed through every system and pass each frame 
	/// 
	/// Rules:
	/// -Renderer populates core fields in Begin Frame 
	/// -Culling System reads scene, writes visibility
	/// -Render Batcher reads Visibility, writes "BatchQueue" via Set<>()
	/// -Passes read "BatchQueue" and write renderTargets / statistics.
	/// -No pass stores pointer to FrameContext across frames
	/// </summary>

    class FrameContext
    {
    public:
        // CORE FRAME DATA
        uint32_t frameNumber = 0;
        FrameTime deltaTime;

        std::shared_ptr<Camera> camera;
        std::shared_ptr<RHI::IGraphicsDevice> device;
        std::shared_ptr<RHI::ICommandBuffer> commandBuffer;

        // SYSTEMS (Shared between passes)
        std::shared_ptr<CullingSystem> cullingSystem;
        std::shared_ptr<RenderBatcher> renderBatcher;
        std::shared_ptr<ResourceCache> resourceCache;

        // SCENE DATA (Submitted by application)
        struct SceneData
        {
            std::vector<std::shared_ptr<Model>> models;
            std::vector<std::shared_ptr<UIElement>> uiElements;

            void Clear()
            {
                models.clear();
                uiElements.clear();
            }
        } scene;

        // RENDER TARGETS (Produced by passes)

        struct RenderTargets
        {
            // GBuffer outputs (for deferred)
            std::shared_ptr<RHI::ITexture> gbufferAlbedo;
            std::shared_ptr<RHI::ITexture> gbufferNormal;
            std::shared_ptr<RHI::ITexture> gbufferDepth;
            std::shared_ptr<RHI::ITexture> gbufferRoughness;
            std::shared_ptr<RHI::ITexture> gbufferMetallic;

            // Shadow maps
            std::shared_ptr<RHI::ITexture> shadowMap;
            std::shared_ptr<RHI::ITexture> shadowCascades[4];

            // Lighting
            std::shared_ptr<RHI::ITexture> lightAccumulation;

            // Final output
            std::shared_ptr<RHI::ITexture> backBuffer;
            std::shared_ptr<RHI::ITexture> depthBuffer;

            // Post-process ping-pong buffers
            std::shared_ptr<RHI::ITexture> pingBuffer;
            std::shared_ptr<RHI::ITexture> pongBuffer;
        } renderTargets;

        // VISIBILITY DATA (Produced by culling)
        struct VisibilityData
        {
            std::vector<const Model*> visibleModels;
            std::vector<const Model*> shadowCasters;
            DirectX::BoundingFrustum viewFrustum;
            uint32_t objectsCulled = 0;

            void Clear()
            {
                visibleModels.clear();
                shadowCasters.clear();
                objectsCulled = 0;
            }
        } visibility;

        struct Statistics
        {
            uint32_t drawCalls = 0;
            uint32_t instanceDrawCalls = 0;
            uint32_t trianglesRendered = 0;
            uint32_t instancesRendered = 0;
            uint32_t pipelineChanges = 0;

            void Reset()
            {
                drawCalls = 0;
                instanceDrawCalls = 0;
                trianglesRendered = 0;
                instancesRendered = 0;
                pipelineChanges = 0;
            }

            void Accumulate(const Statistics& other)
            {
                drawCalls += other.drawCalls;
                instanceDrawCalls += other.instanceDrawCalls;
                trianglesRendered += other.trianglesRendered;
                instancesRendered += other.instancesRendered;
                pipelineChanges += other.pipelineChanges;
            }
        } statistics;

        // GENERIC DATA STORAGE (Blackboard pattern)

        /**
         * Store arbitrary data that passes can share
         *
         * Example:
         *   ctx.Set("ClusteredLightGrid", lightGrid);
         *   auto grid = ctx.Get<LightGrid>("ClusteredLightGrid");
         */
        template<typename T>
        void Set(const std::string& key, const T& value)
        {
            m_Data[key] = value;
        }

        template<typename T>
        T Get(const std::string& key, const T& defaultValue = T()) const
        {
            auto it = m_Data.find(key);
            if (it != m_Data.end())
            {
                try
                {
                    return std::any_cast<T>(it->second);
                }
                catch (const std::bad_any_cast&)
                {
                    return defaultValue;
                }
            }
            return defaultValue;
        }

        bool Has(const std::string& key) const
        {
            return m_Data.find(key) != m_Data.end();
        }

 
        void BeginFrame()
        {
            frameNumber++;
            scene.Clear();
            visibility.Clear();
            statistics.Reset();
        }

        void EndFrame()
        {
            // Cleanup temporary data if needed
        }

    private:
        std::unordered_map<std::string, std::any> m_Data;
    };
}


