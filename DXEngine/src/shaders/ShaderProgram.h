#pragma once
#include <memory>
#include "RHI/GraphicsDevice.h"

namespace DXEngine {

	class ShaderProgram
	{
	public:
		ShaderProgram(std::shared_ptr<RHI::IShader> vs, std::shared_ptr<RHI::IShader> ps);
		~ShaderProgram();

		// A program with NO pixel shader (shadow maps, depth pre-pass): the pipeline only writes depth.
		static std::shared_ptr<ShaderProgram> CreateDepthOnly(std::shared_ptr<RHI::IShader> vs);
		bool IsDepthOnly() const { return m_DepthOnly; }


		RHI::IShader* GetVertexShader() const { return m_VertexShader.get(); }
		RHI::IShader* GetPixelShader() const { return m_PixelShader.get(); }

		bool IsValid() const {
			if (!m_VertexShader || !m_VertexShader->IsCompiled())
				return false;
			// a depth-only program is valid WITHOUT a pixel shader; every other program needs one
			return m_DepthOnly || (m_PixelShader && m_PixelShader->IsCompiled());
		}
	private:
		std::shared_ptr<RHI::IShader> m_VertexShader;
		std::shared_ptr<RHI::IShader> m_PixelShader;
		bool m_DepthOnly = false;
	};

}