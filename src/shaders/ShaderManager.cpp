#include "ShaderManager.h"

namespace DXEngine {

	ShaderManager::ShaderManager()
	{
		m_Programs["Flat"] = std::make_shared<ShaderProgram>(, L"assets/shaders/flatVsShader.cso",
			L"assets/shaders/flatPsShader.cso");
		m_Programs["DiffuseNormal"] = std::make_shared<ShaderProgram>(, L"assets/shaders/vs.cso",
			L"assets/shaders/ps.cso");
		m_Programs["SkyShader"] = std::make_shared<ShaderProgram>(, L"assets/shaders/skyVs.cso",
			L"assets/shaders/skyPs.cso");
	}

	ShaderManager::~ShaderManager()
	{
	}

	void ShaderManager::BindShaders()
	{


	}

	std::shared_ptr<ShaderProgram> ShaderManager::GetShaderProgram(std::string name)
	{
		auto it = m_Programs.find(name);
		return (it != m_Programs.end()) ? it->second : m_Programs["Flat"];
	}
}