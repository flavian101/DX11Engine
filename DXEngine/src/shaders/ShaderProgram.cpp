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
	std::shared_ptr<ShaderProgram> ShaderProgram::CreateDepthOnly(std::shared_ptr<RHI::IShader> vs)
	{
		auto program = std::make_shared<ShaderProgram>(std::move(vs), nullptr);
		program->m_DepthOnly = true;
		return program;
	}
}