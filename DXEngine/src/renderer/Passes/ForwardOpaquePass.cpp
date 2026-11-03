#include "dxpch.h"
#include "ForwardOpaquePass.h"
#include "../../camera/Camera.h"

namespace DXEngine {
	bool ForwardOpaquePass::Initialize(FrameContext& ctx)
	{
		if (!ctx.resourceCache)
		{
			OutputDebugStringA("ERROR: ForwardOpaquePass::Initialize - no ResourceCache in context\n");
			return false;
		}

		//create persistent constant buffers through the cache
		m_PerFrameCB = ctx.resourceCache->GetOrCreateConstantBuffer(
			"ForwardPass_PerFrame", sizeof(PerFrameConstants));

		m_PerObjectCB = ctx.resourceCache->GetOrCreateConstantBuffer(
			"ForwardPass_PerObject", sizeof(PerObjectConstants));

		if (!m_PerFrameCB || !m_PerObjectCB)
		{
			OutputDebugStringA("ERROR: ForwardOpaquePass::Initialize - failed to create constant buffers\n");
			return false;
		}

		OutputDebugStringA("ForwardOpaquePass initialized\n");
		return true;
		
	}
	//runs before execute, safe to do cpu work here 
	void ForwardOpaquePass::Prepare(FrameContext& ctx)
	{
		m_DrawCalls = 0;
		m_TrianglesDrawn = 0;
		m_LastMaterial = nullptr;
		m_LastPipeline = nullptr;

		// Upload per-frame constants while GPU is still busy from last frame
		UpdatePerFrameBuffer(ctx);
	}
	void ForwardOpaquePass::Execute(FrameContext & ctx)
	{
		auto* cmd = ctx.commandBuffer.get();
		if (!cmd)
			return;

		//retrieve the sorted batch queue built by renderBatcher
		BatchQueue queue = ctx.Get<BatchQueue>("BatchQueue");
		if (queue.opaque.empty())
			return;

		//set render target (backBuffer + depth Buffer)
		SetRenderTargets(ctx);

		// Bind per-frame constants once for all draws
		cmd->SetConstantBuffer(m_PerFrameCB.get(), 0, RHI::ShaderStage::Vertex);
		cmd->SetConstantBuffer(m_PerFrameCB.get(), 0, RHI::ShaderStage::Pixel);

		//Draw every opaque item(already sorted front to back)
		for (const auto& item : queue.opaque)
		{
			DrawItem(cmd, item, ctx);
		}

	}
	void ForwardOpaquePass::Finalize(FrameContext & ctx)
	{}
	void ForwardOpaquePass::ShutDown()
	{}
	void ForwardOpaquePass::OnResize(uint32_t width, uint32_t height)
	{}
	std::string ForwardOpaquePass::GetStatistics() const
	{
		return std::string();
	}
	void ForwardOpaquePass::SetRenderTargets(FrameContext& ctx)
	{
		auto* cmd = ctx.commandBuffer.get();
		if (!cmd)return;

		auto& rt = ctx.renderTargets;
		cmd->SetRenderTarget(rt.backBuffer.get(), rt.depthBuffer.get());
		if (rt.backBuffer)
		{
			RHI::ViewportDesc vp;
			vp.x = 0.0f;
			vp.y = 0.0f;
			vp.width = static_cast<float>(rt.backBuffer->GetWidth());
			vp.height = static_cast<float>(rt.backBuffer->GetHeight());
			vp.minDepth = 0.0f;
			vp.maxDepth = 1.0f;
			cmd->SetViewport(vp);
		}

	}
	void ForwardOpaquePass::UpdatePerFrameBuffer(FrameContext & ctx)
	{
		if (!ctx.camera || !m_PerFrameCB)
			return;

		PerFrameConstants data;
		data.View = DirectX::XMMatrixTranspose(ctx.camera->GetViewMatrix());
		data.Projection = DirectX::XMMatrixTranspose(ctx.camera->GetProjectionMatrix());
		data.ViewProjection = DirectX::XMMatrixTranspose(
			ctx.camera->GetViewMatrix() * ctx.camera->GetProjectionMatrix());
		data.CameraPosition = ctx.camera->GetPositionFloat3();
		data.Time = ctx.deltaTime.Duration(); ///confirm this is the same as total time

		auto& rt = ctx.renderTargets;
		data.ScreenSize = { 0.0f,0.0f };
		if (rt.backBuffer)
		{
			data.ScreenSize.x = static_cast<float>(rt.backBuffer->GetWidth());
			data.ScreenSize.y = static_cast<float>(rt.backBuffer->GetHeight());
		}

		data.NearPlane = ctx.camera->GetNearPlane();
		data.FarPlane = ctx.camera->GetFarPlane();

		m_PerFrameCB->Update(&data, sizeof(PerFrameConstants), 0);
	}
	void ForwardOpaquePass::DrawItem(RHI::ICommandBuffer * cmd, const DXEngine::DrawItem & item, FrameContext & ctx)
	{
		if (!item.mesh || !item.mesh->IsValid() || !item.material)
			return;

		//Pipeline(Cached via ResourceCache)
		auto pipeline = GetPipeline(item, ctx);
		if(!)
	}
	void ForwardOpaquePass::UploadPerObjectData(RHI::ICommandBuffer* cmd, const DXEngine::DrawItem& item, FrameContext& ctx)
	{}
	void ForwardOpaquePass::UploadBoneData(RHI::ICommandBuffer * cmd, const DXEngine::DrawItem & item, FrameContext & ctx)
	{}
	void ForwardOpaquePass::BindMaterial(RHI::ICommandBuffer * cmd, const DXEngine::DrawItem & item, FrameContext & ctx)
	{}
	std::shared_ptr<RHI::IPipeline> ForwardOpaquePass::GetPipeline(const DXEngine::DrawItem & item, FrameContext & ctx)
	{
		return std::shared_ptr<RHI::IPipeline>();
	}
}