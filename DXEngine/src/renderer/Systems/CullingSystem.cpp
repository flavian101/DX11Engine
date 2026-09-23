#include "dxpch.h"
#include "CullingSystem.h"
#include "../../camera/Camera.h"

namespace DXEngine
{
	void CullingSystem::Cull(FrameContext& ctx)
	{
		//clear lasts frame's results
		ctx.visibility.visibleModels.clear();
		ctx.visibility.shadowCasters.clear();
		ctx.visibility.objectsCulled = 0;

		m_LastVisible = 0;
		m_LastCulled = 0;


		if (ctx.scene.models.empty())
		{
			return;
		}

		//Build view frustrum once for the whole frame
		ctx.visibility.viewFrustum = BuildFrustum(ctx);

		for (const auto& model : ctx.scene.models)
		{
			if (!model || !model->IsValid() || !model->IsVisible())
				continue;

			CullResult result = TestModel(model.get(), ctx);

			if (result == CullResult::Visible)
			{
				ctx.visibility.visibleModels.push_back(model.get());

				//shadow casters are a separate list - shadows pass uses a wider frustrum
				if (model->CastsShadows())
					ctx.visibility.shadowCasters.push_back(model.get());

				m_LastVisible++;
			}
			else
			{
				ctx.visibility.objectsCulled++;
				m_LastCulled++;
			}
		}


	}
	
	CullResult CullingSystem::TestModel(const Model* model, const FrameContext& ctx) const
	{
		if (!model)
			return CullResult::FrustumCulled;

		//Distance test first(cheapest)
		if (m_Config.enableDistanceCulling)
		{
			CullResult dist = TestDistance(model, ctx);
			if (dist != CullResult::Visible)
				return dist;
		}

		//Frustum Test
		if (m_Config.enableFrustumCulling)
		{
			CullResult frustum = TestFrustum(model, ctx.visibility.viewFrustum);
			if (frustum != CullResult::Visible)
			{
				return frustum;
			}
		}
		return CullResult::Visible;
	}

	CullResult CullingSystem::TestFrustum(const Model* model, const DirectX::BoundingFrustum& frustum) const
	{
		//getWorldBoundingSphere already accounts for mode  transform + scale
		BoundingSphere sphere = model->GetWorldBoundingSphere();

		//convert your Bounding sphere to DirectX type
		DirectX::BoundingSphere dxSphere;
		dxSphere.Center = sphere.center;
		dxSphere.Radius = sphere.radius * m_Config.frustumMarginScale;

		DirectX::ContainmentType result = frustum.Contains(dxSphere);

		if (result == DirectX::DISJOINT)
			return CullResult::FrustumCulled;
		return CullResult::Visible;
	}
	CullResult CullingSystem::TestDistance(const Model* model, const FrameContext& ctx) const
	{
		if (!ctx.camera)
			return CullResult::Visible;

		BoundingSphere sphere = model->GetWorldBoundingSphere();

		DirectX::XMFLOAT3 camPos = ctx.camera->GetPositionFloat3();
		DirectX::XMVECTOR cam = DirectX::XMLoadFloat3(&camPos);
		DirectX::XMVECTOR center = DirectX::XMLoadFloat3(&sphere.center);

		float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(center, cam)));

		//subtract radius so large objects stay visible when their edge is in range
		if ((distance - sphere.radius) > m_Config.maxDrawDistance)
			return CullResult::DistanceCulled;

		return CullResult::Visible;
	}

	DirectX::BoundingFrustum CullingSystem::BuildFrustum(const FrameContext& ctx) const
	{
		DirectX::BoundingFrustum frustum;

		if (!ctx.camera)
			return frustum;

		DirectX::BoundingFrustum::CreateFromMatrix(frustum, ctx.camera->GetProjectionMatrix());

		// Transform frustum into world space using the inverse view matrix
		DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(nullptr, ctx.camera->GetViewMatrix());
		frustum.Transform(frustum, invView);

		return frustum;
	}

	std::string CullingSystem::GetLastFrameStats() const
	{
		std::ostringstream oss;
		oss << "CullingSystem:\n";
		oss << "  Visible:  " << m_LastVisible << "\n";
		oss << "  Culled:   " << m_LastCulled << "\n";
		uint32_t total = m_LastVisible + m_LastCulled;
		if (total > 0)
		{
			float pct = (static_cast<float>(m_LastCulled) / total) * 100.0f;
			oss << "  Efficiency: " << pct << "% culled\n";
		}
		return oss.str();
	}
}