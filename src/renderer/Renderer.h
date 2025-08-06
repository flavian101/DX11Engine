#pragma once
#include <Camera.h>
#include <models/Model.h>

namespace DXEngine {

	class Renderer
	{
	public:
		static void Init();
		static void BeginScene(Camera& camera);
		static void EndScene();


		static void Submit(Model& model,std::shared_ptr<ShaderProgram> program);

	private:
	};
}
