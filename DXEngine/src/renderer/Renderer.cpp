#include "dxpch.h"
#include "Renderer.h"
#include <utils/Material.h>
#include "models/Model.h"
#include "utils/Mesh.h"
#include "utils/ConstantBuffer.h"




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

	void Renderer::BeginScene(const std::shared_ptr<Camera>& camera)
	{
		RenderCommand::SetCamera(camera);
		RenderCommand::Clear();
		ResetStats();
	}

	void Renderer::Submit(const std::shared_ptr<Model>& model)
	{
		if (!model->GetMesh())
			return;

		RenderMesh(model->GetMesh(), model->GetModelMatrix(), model->GetMesh()->GetMaterial());

	}

	void Renderer::Submit(const std::shared_ptr<Model>& model, const std::shared_ptr<Material>& materialOverride)
	{

		if (!model->GetMesh())
			return;

		RenderMesh(model->GetMesh(), model->GetModelMatrix(), materialOverride);

		s_Stats.DrawCalls++;
	}

	void Renderer::Submit(const std::shared_ptr<Mesh>& mesh, const DirectX::XMMATRIX& transform)
	{
		if (!mesh)
			return;

		RenderMesh(mesh, transform, mesh->GetMaterial());
	}

	void Renderer::Submit(const std::shared_ptr<Mesh>& mesh, const DirectX::XMMATRIX& transform, const std::shared_ptr<Material>& material)
	{
		if (!mesh)
			return;

		RenderMesh(mesh, transform, material);
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
		//RenderCommand::GetCamera().   //set the projection of the new camera TO-DO
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
	void Renderer::RenderMesh(const std::shared_ptr<Mesh>& mesh, const DirectX::XMMATRIX& transform, const std::shared_ptr<Material>& material)
	{
		ConstantBuffer<TransfomBufferData> vsBuffer;
		vsBuffer.Initialize();
		vsBuffer.data.WVP = XMMatrixTranspose(transform * RenderCommand::GetCamera().GetView() * RenderCommand::GetCamera().GetProjection());
		vsBuffer.data.Model = XMMatrixTranspose(transform);
		vsBuffer.Update();
		RenderCommand::GetContext()->VSSetConstantBuffers(BindSlot::CB_Transform, 1, vsBuffer.GetAddressOf());

		if (material)
			material->Bind();

		mesh->BindBuffers();

		//mesh->GetTopology()Bind();

		RenderCommand::DrawIndexed(mesh->GetIndexCount());

		s_Stats.DrawCalls++;
		s_Stats.IndexCount += mesh->GetIndexCount();

		//std::cout << "draw calls" << " " << s_Stats.DrawCalls << std::endl;
		std::cout << "Index Count" << " " << s_Stats.IndexCount<< std::endl;

	}
}