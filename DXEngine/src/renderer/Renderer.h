#pragma once
#include "Camera.h"
#include "RendererCommand.h"
#include <memory>

namespace DXEngine {

	// Forward declarations
	class ShaderProgram;
	class Model;
	class Mesh;
	class Material;

	class Renderer
	{
	public:
		static void Init(HWND hwnd, int width, int height);
		static void Shutdown();

		static void BeginScene(const std::shared_ptr<Camera>& camera);
		static void EndScene();

		static void Submit(const std::shared_ptr<Model>& model, const std::shared_ptr<Material>& materialOverride);
		static void Submit(const std::shared_ptr<Model>& model);
		static void Submit(const std::shared_ptr<Mesh>& mesh, const DirectX::XMMATRIX& transform);
		static void Submit(const std::shared_ptr<Mesh>& mesh, const DirectX::XMMATRIX& transform, const std::shared_ptr<Material>& material);


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
		static void RenderMesh(const std::shared_ptr<Mesh>& mesh, const DirectX::XMMATRIX& transform, const std::shared_ptr<Material>& material);
		static Statistics s_Stats;
	};
}