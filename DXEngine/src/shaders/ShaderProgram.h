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
		~ShaderProgram();

		ID3DBlob* GetByteCode();

		void Bind();
	private:
		std::shared_ptr<VertexShader> m_VertexShader;
		std::shared_ptr <PixelShader> m_PixelShader;;
	};

}