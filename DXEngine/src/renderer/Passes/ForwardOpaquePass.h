#pragma once
#include "../Core/RenderPass.h"
#include "../Core/FrameContext.h"
#include "../Systems/RenderBatcher.h"
#include "../Cache/ResourceCache.h"
#include <DirectXMath.h>

namespace DXEngine {

	//constant buffer

	struct alignas(16) PerFrameConstants
	{
		DirectX::XMMATRIX View;
		DirectX::XMMATRIX Projection;
		DirectX::XMMATRIX ViewProjection;
		DirectX::XMFLOAT3 CameraPosition;
		float             Time;
		DirectX::XMFLOAT2 ScreenSize;
		float             NearPlane;
		float             FarPlane;
	};

	struct alignas(16)PerObjectConstants
	{
		DirectX::XMMATRIX World;
		DirectX::XMMATRIX WorldInverseTranspose;  // Normal matrix
		DirectX::XMMATRIX WVP;
	};

	struct alignas(16) BoneConstants
	{
		DirectX::XMMATRIX BoneMatrices[128];
	};

	//ForwardOpaque pass
	//Renders all opaque DrawItems produced be Render batcher
	//Reads: ctx.BatchQueue.opaque
	//Writes: ctx.renderTargets.backBuffer(color), depthBuffer(depth)

	class ForwardOpaquePass: public RenderPass
	{
	public:
		ForwardOpaquePass() = default;
		~ForwardOpaquePass()override = default;

		//RenderPass Interface

		bool Initialize(FrameContext& ctx)override;
		void Prepare(FrameContext& ctx)override;
		void Execute(FrameContext& ctx) override;
		void Finalize(FrameContext& ctx) override;
		void ShutDown() override;
		void OnResize(uint32_t width, uint32_t height)override;
		std::string GetName()const override { return "ForwardOpaquePass"; }
		std::string GetStatistics()const override;

	private:
		//per-pass helpers
		void SetRenderTargets(FrameContext& ctx);
		void UpdatePerFrameBuffer(FrameContext& ctx);
		void DrawItem(RHI::ICommandBuffer* cmd, const DXEngine::DrawItem& item, FrameContext& ctx);

		void UploadPerObjectData(RHI::ICommandBuffer* cmd, const DXEngine::DrawItem& item, FrameContext& ctx);
		void UploadBoneData(RHI::ICommandBuffer* cmd, const DXEngine::DrawItem& item, FrameContext& ctx);
		void BindMaterial(RHI::ICommandBuffer* cmd, const DXEngine::DrawItem& item, FrameContext& ctx);

		std::shared_ptr<RHI::IPipeline> GetPipeline(
			const DXEngine::DrawItem& item,
			FrameContext& ctx);

	private:
		// Persistent constant buffers (owned via ResourceCache)
		std::shared_ptr<RHI::IBuffer> m_PerFrameCB;
		std::shared_ptr<RHI::IBuffer> m_PerObjectCB;

		//cached last-bound state - avoids redundant GPU calls
		Material* m_LastMaterial = nullptr;
		RHI::IPipeline* m_LastPipeline = nullptr;

		//stats
		uint32_t m_DrawCalls = 0;
		uint32_t m_TrianglesDrawn = 0;

	};

}

