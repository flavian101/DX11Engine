#pragma once
#include "..\utils\VertexShader.h"
#include "..\utils\PixelShader.h"
#include <memory>
#include "..\Graphics.h"
#include <utils/InputLayout.h>



class ShaderProgram
{
public:
	ShaderProgram(Graphics& gfx,LPCWSTR vertexShader, LPCWSTR pixelShader);
	~ShaderProgram();

	ID3DBlob* GetByteCode();

	void Bind(Graphics& gfx);
private:
	std::shared_ptr<InputLayout> layout;
	std::shared_ptr<VertexShader> m_VertexShader;
	std::shared_ptr <PixelShader> m_PixelShader;;
};

