#pragma once
#include "..\utils\VertexShader.h"
#include "..\utils\PixelShader.h"
#include <memory>
#include "renderer/Renderer.h"

namespace DXEngine {


	class ShaderProgram
	{
	public:
		ShaderProgram( LPCWSTR vertexShader, LPCWSTR pixelShader);
		ShaderProgram(std::shared_ptr<VertexShader> vs, std::shared_ptr<PixelShader> ps);

		~ShaderProgram();

		ID3DBlob* GetByteCode();

		void Bind();
	private:
		std::shared_ptr<VertexShader> m_VertexShader;
		std::shared_ptr <PixelShader> m_PixelShader;;
	};

}