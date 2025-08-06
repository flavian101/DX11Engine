#include "Renderer.h"

namespace DXEngine {

	Renderer::Statistics Renderer::s_Stats;

	void Renderer::Init(HWND hwnd, int width, int height)
	{
		RenderCommand::Init(hwnd, width, height);
		ResetStats();
	}

	void Renderer::Shutdown()
	{
		RenderCommand::Shutdown();
	}

	void Renderer::BeginScene(Camera& camera)
	{
		RenderCommand::SetCamera(std::make_shared<Camera>(camera));
		RenderCommand::Clear();
		ResetStats();
	}

	void Renderer::Submit(Model& model, std::shared_ptr<ShaderProgram> program)
	{
		// This is where you would bind the model's vertex/index buffers,
		// set the shader program, update constant buffers with model matrices, etc.
		// For now, this is a placeholder that shows the structure

		// Example of what would happen here:
		// 1. Bind vertex and index buffers from the model
		// 2. Set the shader program
		// 3. Update constant buffers (world matrix, view matrix, projection matrix)
		// 4. Set textures and samplers
		// 5. Call RenderCommand::DrawIndexed()

		// Update statistics
		s_Stats.DrawCalls++;
		// s_Stats.IndexCount += model.GetIndexCount();
		// s_Stats.VertexCount += model.GetVertexCount();

		// RenderCommand::DrawIndexed(model.GetIndexCount());
	}

	void Renderer::EndScene()
	{
		RenderCommand::Present();
	}

	void Renderer::SetClearColor(float red, float green, float blue, float alpha)
	{
		RenderCommand::SetClearColor(red, green, blue, alpha);
	}

	void Renderer::OnWindowResize(int width, int height)
	{

		float aspect = height / width;
		DirectX::XMMATRIX camProjection = DirectX::XMMatrixPerspectiveLH(1.0f, aspect, 0.5f, 100.0f);
		//RenderCommand::GetCamera().   //set the projection of the new camera
		RenderCommand::Resize(width, height);
	}

	void Renderer::ResetStats()
	{
		memset(&s_Stats, 0, sizeof(Statistics));
	}

	Renderer::Statistics Renderer::GetStats()
	{
		return s_Stats;
	}
}