#pragma once
#include "ShaderProgram.h"
#include <unordered_map>
#include <string>

namespace DXEngine {

	class ShaderManager
	{
	public:
		ShaderManager();

		~ShaderManager();

		void BindShaders();

		std::shared_ptr<ShaderProgram> GetShaderProgram(const std::string& name);

	private:
		std::unordered_map<std::string, std::shared_ptr<ShaderProgram>> m_Programs;
	};

}