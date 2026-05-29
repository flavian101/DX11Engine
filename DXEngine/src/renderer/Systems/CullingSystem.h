#pragma once
#include "models/Model.h"
#include "camera/Camera.h"
#include "renderer/core/FrameContext.h"
#include <DirectXCollision.h>
#include <vector>
#include <memory>
namespace DXEngine::Rendering
{
	//cull result 
	//Returned per-Model so callers know exatly why a model was culled
	enum class CullResult : uint8_t
	{
		Visible,
		FrustumCulled,
		DistanceCulled
	};

	//cull config
	struct CullingConfig
	{
		bool enableFrustumCulling = true;
		bool enableDistanceCulling = false;
		float maxDrawDistance = 1000.0f; // in World units
		float frustumMarginScale = 1.05f; // safty margin on bounding spheres
	};

	//Culling System: Reads from FrameContext::Scene and writes tho FrameContext Visbility

	class CullingSystem
	{
	public:
		CullingSystem() = default;
		explicit CullingSystem(const CullingConfig& config): m_Config(config){}

		//after submit has been called to populate the visiblility scene from the ctx scene
		void Cull(FrameContext& ctx);

		//Single-object Test(usefull for Shadow casters, light volumes)
		CullResult TestModel(const Model* model, const FrameContext& ctx)const;

		void SetConfig(const CullingConfig& config) { m_Config = config; }
		const CullingConfig& GetConfig() const { return m_Config; }

		std::string GetLastFrameStats() const;

	private:
		DirectX::BoundingFrustum BuildFrustum(const FrameContext& ctx)const;
		CullResult TestFrustum(const Model* model, const DirectX::BoundingFrustum& frustum)const;
		CullResult TestDistance(const Model* model, const FrameContext& ctx) const;


	private:
		CullingConfig m_Config;
		// Per-frame stats (reset each call to Cull)
		uint32_t m_LastVisible = 0;
		uint32_t m_LastCulled = 0;
	};
}
