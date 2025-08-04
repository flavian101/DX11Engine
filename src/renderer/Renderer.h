#pragma once
#include <Camera.h>
#include <models/Model.h>

namespace DXEngine {

	class Renderer
	{
	public:
		void BeginScene(Camera& camera);

		void Submit(Model& model);
		void EndScene();
	private:
	};
}
