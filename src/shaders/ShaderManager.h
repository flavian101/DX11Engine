#pragma once
#include "ShaderProgram.h"
#include <unordered_map>

namespace DXEngine {

	class ShaderManager
	{
	public:
		ShaderManager();

		~ShaderManager();

		void BindShaders();

		std::shared_ptr<ShaderProgram> GetShaderProgram(std::string name);

	private:
		std::unordered_map<std::string, std::shared_ptr<ShaderProgram>> m_Programs;
	};

}