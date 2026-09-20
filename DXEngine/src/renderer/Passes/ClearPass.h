#pragma once
#include "../Core/RenderPass.h"
#include "../Core/FrameContext.h"
#include "../../RHI/GraphicsDevice.h"


namespace DXEngine {

	///Clear pass
	//responsibility: clear color and depth before any geometry renders.
	//always the first pass in any forward graph

	//reads: ctx.renderTargets.backBuffer / depthBuffer
	//Writes: nothing (side-effect: clears GPU buffers


	class ClearPass final : public RenderPass
	{
	public:
		struct Config
		{
			float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
			float clearDepth = 1.0f;
			uint8_t clearStencil = 0;
			bool clearColor = true;
			bool clearDepth_ = true;
		};

		explicit ClearPass(const Config& cfg={}): m_Config(cfg){}

		void Execute(FrameContext& ctx)
		{
			auto* cmd = ctx.commandBuffer.get();
			auto& rt = ctx.renderTargets;

			if (!cmd || !rt.backBuffer)
			{
				return;
			}

			cmd->SetRenderTarget(rt.backBuffer.get(),
				rt.depthBuffer ? rt.depthBuffer.get() : nullptr);

			if (m_Config.clearColor)
			{
				cmd->ClearRenderTarget(rt.backBuffer.get(),
					m_Config.clearColor[0],
					m_Config.clearColor[1],
					m_Config.clearColor[2],
					m_Config.clearColor[3]
				);
			}
			if (m_Config.clearDepth_ && rt.depthBuffer)
			{
				cmd->ClearDepthStencil(rt.depthBuffer.get(),
					m_Config.clearDepth,
					m_Config.clearStencil);
			}
		}

		std::string GetName()const override { return "Clear pass"; }

		//expose config so Renderer can forward setClearColor
		void SetColor(float r, float g, float b, float a)
		{
			m_Config.clearColor[0] = r;
			m_Config.clearColor[1] = g;
			m_Config.clearColor[2] = b;
			m_Config.clearColor[3] = a;
		}

	private:
		Config m_Config;

	};
}