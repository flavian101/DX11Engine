#pragma once
#include <memory>
#include "RHI/GraphicsDevice.h"

namespace DXEngine {

	class ShaderProgram
	{
	public:
		ShaderProgram(std::shared_ptr<RHI::IShader> vs, std::shared_ptr<RHI::IShader> ps);
		~ShaderProgram();


		RHI::IShader* GetVertexShader() const { return m_VertexShader.get(); }
		RHI::IShader* GetPixelShader() const { return m_PixelShader.get(); }

		bool IsValid() const {
			return m_VertexShader && m_PixelShader &&
				m_VertexShader->IsCompiled() && m_PixelShader->IsCompiled();
		}
	private:
		std::shared_ptr<RHI::IShader> m_VertexShader;
		std::shared_ptr<RHI::IShader> m_PixelShader;;
	};

}