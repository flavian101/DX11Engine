#include "dxpch.h"
#include "CullingSystem.h"

namespace DXEngine::Rendering
{
	void CullingSystem::Cull(FrameContext& ctx)
	{
		//clear lasts frame's results
	}
	
	CullResult CullingSystem::TestModel(const Model* model, const FrameContext& ctx) const
	{
		return CullResult();
	}
	std::string CullingSystem::GetLastFrameStats() const
	{
		return std::string();
	}
	DirectX::BoundingFrustum CullingSystem::BuildFrustum(const FrameContext& ctx) const
	{
		return DirectX::BoundingFrustum();
	}
	CullResult CullingSystem::TestFrustum(const Model* model, const DirectX::BoundingFrustum& frustum) const
	{
		return CullResult();
	}
	CullResult CullingSystem::TestDistance(const Model* model, const FrameContext& ctx) const
	{
		return CullResult();
	}
}