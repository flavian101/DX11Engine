#pragma once
#include "Camera.h"
#include "models/Model.h"
#include "RendererCommand.h"
#include <memory>

namespace DXEngine {

	// Forward declarations
	class ShaderProgram;

	class Renderer
	{
	public:
		static void Init(HWND hwnd, int width, int height);
		static void Shutdown();

		static void BeginScene(Camera& camera);
		static void EndScene();

		static void Submit(Model& model, std::shared_ptr<ShaderProgram> program);

		// Scene management
		static void SetClearColor(float red, float green, float blue, float alpha = 1.0f);

		// Window management
		static void OnWindowResize(int width, int height);

		// Statistics
		struct Statistics
		{
			uint32_t DrawCalls = 0;
			uint32_t VertexCount = 0;
			uint32_t IndexCount = 0;
		};

		static void ResetStats();
		static Statistics GetStats();

	private:
		static Statistics s_Stats;
	};
}