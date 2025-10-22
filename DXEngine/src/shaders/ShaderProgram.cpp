#include "dxpch.h"
#include "ShaderProgram.h"
namespace DXEngine {
	ShaderProgram::ShaderProgram(std::shared_ptr<RHI::IShader> vs, std::shared_ptr<RHI::IShader> ps)
		:
		m_VertexShader(vs),
		m_PixelShader(ps)
	{
	}
	ShaderProgram::~ShaderProgram()
	{
	}
}