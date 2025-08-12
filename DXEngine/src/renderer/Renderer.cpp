#include "dxpch.h"
#include "Renderer.h"
#include "models/Model.h"
#include "utils/Mesh/Mesh.h"
#include "utils/ConstantBuffer.h"
#include "Camera.h"
#include "shaders/ShaderManager.h"
#include <algorithm>
#include "utils/UI/UIElement.h"
#include <utils/UI/UIButton.h>
#include <utils/UI/UIPanel.h>


namespace DXEngine {

	Renderer::RenderStatistics Renderer::s_Stats;
	std::shared_ptr<ShaderManager> Renderer::s_ShaderManager = nullptr;
	std::vector<DXEngine::RenderItem> Renderer::s_RenderItems;
	std::map<RenderQueue, std::vector<DXEngine::RenderItem*>> Renderer::s_SortedQueues;
	std::shared_ptr<Material> Renderer::s_CurrentMaterial = nullptr;
	std::shared_ptr<ShaderProgram> Renderer::s_CurrentShader = nullptr;
	MaterialType Renderer::s_CurrentMaterialType = MaterialType::Unlit;
	bool Renderer::s_WireframeEnabled = false;
	bool Renderer::s_DebugInfoEnabled = false;
	uint32_t Renderer::s_FrameCount = 0;

	std::shared_ptr<Model> Renderer::s_UIQuadModel = nullptr;
	std::shared_ptr<Material> Renderer::s_DefaultUIMaterial = nullptr;
	DirectX::XMMATRIX Renderer::s_UIProjectionMatrix = DirectX::XMMatrixIdentity();
	std::vector<Renderer::RenderState> Renderer::s_RenderStateStack;



	void Renderer::Init(HWND hwnd, int width, int height)
	{
		RenderCommand::Init(hwnd, width, height);

		CreateUIQuad();
		CreateDefaultUIMaterial();
		UpdateUIProjectionMatrix(width, height);

		ResetStats();
	}

	void Renderer::InitShaderManager(std::shared_ptr<ShaderManager> shaderManager)
	{
		s_ShaderManager = shaderManager;
		if (!s_ShaderManager)
		{
			OutputDebugStringA("Warning: RenderSystem initialized without ShaderManager!\n");
		}
		s_RenderItems.reserve(1000);//reserve space for render items to avoid infrequent allocations
	}

	void Renderer::Shutdown()
	{
		s_RenderItems.clear();
		s_SortedQueues.clear();
		s_ShaderManager.reset();
		s_CurrentMaterial.reset();
		s_CurrentShader.reset();
		RenderCommand::Shutdown();

	}

	void Renderer::BeginScene(const std::shared_ptr<Camera>& camera)
	{
		RenderCommand::SetCamera(camera);
		s_RenderItems.clear();
		s_SortedQueues.clear();
		s_CurrentMaterial.reset();
		s_CurrentShader.reset();
		s_CurrentMaterialType = MaterialType::Unlit;
		ResetStats();


		RenderCommand::Clear();

		// Check for shader hot reload in debug builds
#ifdef _DEBUG
		if (s_ShaderManager)
		{
			s_ShaderManager->CheckForShaderChanges();
		}
#endif
	}

	void Renderer::EndScene()
	{
		ProcessRenderQueue();

		RenderCommand::Present();

		if (s_DebugInfoEnabled)
		{
			OutputDebugStringA(GetDebugInfo().c_str());
		}
	}

	void Renderer::SetClearColor(float red, float green, float blue, float alpha)
	{
		RenderCommand::SetClearColor(red, green, blue, alpha);
	}

	void Renderer::Submit(std::shared_ptr<Model> model)
	{
		if (!model || !model->GetMesh())
			return;
		auto material = model->GetMesh()->GetMaterial();
		if (!material)
		{
			material = MaterialFactory::CreateLitMaterial("DefaultModelMaterial");
			model->GetMesh()->SetMaterial(material);
		}

		DXEngine::RenderItem item(model, material);
		ValidateRenderItem(item);
		s_RenderItems.push_back(item);

	}
	void Renderer::Submit(std::shared_ptr<Model> model, std::shared_ptr<Material> materialOverride)
	{
		if (!model || !model->GetMesh())
			return;

		auto baseMaterial = model->GetMesh()->GetMaterial();
		if (!baseMaterial)
		{
			baseMaterial = MaterialFactory::CreateLitMaterial("DefaultModelMaterial");
		}

		DXEngine::RenderItem item(model, baseMaterial);
		item.materialOverride = materialOverride;
		item.queue = materialOverride ? materialOverride->GetRenderQueue() : baseMaterial->GetRenderQueue();

		ValidateRenderItem(item);
		s_RenderItems.push_back(item);
	}
	void Renderer::SubmitImmediate(std::shared_ptr<Model> model, std::shared_ptr<Material> material)//TO-DO model already has material thus material parameter may not be needed 
	{
		if (!model || !model->GetMesh())
			return;

		material = model->GetMesh()->GetMaterial();

		if (!material)
		{
			material = MaterialFactory::CreateLitMaterial("ImmediateMaterial");
		}

		DXEngine::RenderItem item(model, material);
		ValidateRenderItem(item);
		Render3DItem(item);

	}
	void Renderer::ProcessRenderQueue()
	{
		if (s_RenderItems.empty())
			return;

		//sort renderItems
		SortRenderItems();

		//render each queue in order
		std::vector<RenderQueue> queueOrder = {
			RenderQueue::Background,
			RenderQueue::Opaque,
			RenderQueue::Transparent,
			RenderQueue::UI,
			RenderQueue::Overlay
		};

		for (RenderQueue queue : queueOrder)
		{
			auto queueIt = s_SortedQueues.find(queue);
			if (queueIt != s_SortedQueues.end())
			{

				SetRenderStateForQueue(queue);

				// Render all items in this queue
				for (DXEngine::RenderItem* item : queueIt->second)
				{
					RenderItem(*item);
				}

			}
		}
		Set3DRenderState();
	}
	void Renderer::SortRenderItems()
	{
		//calculate sort keys
		for (auto& item : s_RenderItems)
		{
			item.sortKey = CalculateSortKey(item);
			s_SortedQueues[item.queue].push_back(&item);
		}

		//sort each queue appropriately
		for (auto& [queue, items] : s_SortedQueues)
		{
			switch (queue)
			{
			case RenderQueue::Background:
			case RenderQueue::Opaque:
			{
				// Sort front to back for early Z rejection
				std::sort(items.begin(), items.end(),
					[](const DXEngine::RenderItem* a, const DXEngine::RenderItem* b)
					{
						return a->sortKey < b->sortKey;
					});
				break;
			}
			case RenderQueue::Transparent:
			{
				// Sort back to front for proper alpha blending
				std::sort(items.begin(), items.end(),
					[](const DXEngine::RenderItem* a, const DXEngine::RenderItem* b) {
						return a->sortKey > b->sortKey;
					});
				break;
			}
			case RenderQueue::Overlay:
				// No depth sorting needed for overlays
				break;

			}
		}
	}

	void Renderer::RenderItem(const DXEngine::RenderItem& item)
	{
		if (item.isUIElement)
		{
			RenderUIItem(item);
		}
		else
		{
			Render3DItem(item);
		}
	}

	void Renderer::Render3DItem(const DXEngine::RenderItem& item)
	{
		if (!item.model || !item.model->GetMesh())
			return;

		auto materialToUse = item.materialOverride ? item.materialOverride : item.material;
		if (!materialToUse)
			return;

		//Bind Shader
		BindShaderForMaterial(materialToUse);

		//Bind material
		BindMaterial(materialToUse);

		//Bind Transform buffer
		ConstantBuffer<TransfomBufferData> vsBuffer;
		vsBuffer.Initialize();
		DirectX::XMMATRIX modelMatrix = item.model->GetModelMatrix();

		auto view = RenderCommand::GetCamera()->GetView();
		auto proj = RenderCommand::GetCamera()->GetProjection();
		vsBuffer.data.WVP = DirectX::XMMatrixTranspose(modelMatrix * view * proj);
		vsBuffer.data.Model = DirectX::XMMatrixTranspose(modelMatrix);
		vsBuffer.Update();

		RenderCommand::GetContext()->VSSetConstantBuffers(BindSlot::CB_Transform, 1, vsBuffer.GetAddressOf());

		//bind mesh
		auto mesh = item.model->GetMesh();
		mesh->BindBuffers();

		RenderCommand::DrawIndexed(mesh->GetIndexCount());

		// Update statistics
		s_Stats.drawCalls++;
		s_Stats.trianglesRendered += mesh->GetIndexCount() / 3;
	}

	void Renderer::RenderUIItem(const DXEngine::RenderItem& item)
	{
		if (!item.uiElement)
			return;

		// Save current render state
		PushRenderState();

		// Set UI render state
		SetUIRenderState();

		// Get UI element bounds and properties
		const UIRect& bounds = item.uiElement->GetBounds();

		// Create transformation matrix for this UI element
		DirectX::XMMATRIX scaleMatrix = DirectX::XMMatrixScaling(bounds.width, bounds.height, 1.0f);
		DirectX::XMMATRIX translationMatrix = DirectX::XMMatrixTranslation(bounds.x, bounds.y, 0.0f);
		DirectX::XMMATRIX modelMatrix = scaleMatrix * translationMatrix;

		// Bind UI material
		auto materialToUse = item.materialOverride ? item.materialOverride : s_DefaultUIMaterial;

		if (auto button = std::dynamic_pointer_cast<UIButton>(item.uiElement))
		{
			// Create dynamic material properties based on button state
			UIColor buttonColor = GetButtonColorForState(button);

			// Update material color (you might want to create a separate UI material instance)
			materialToUse->SetDiffuseColor({ buttonColor.r, buttonColor.g, buttonColor.b, buttonColor.a });
		}

		else if (auto panel = std::dynamic_pointer_cast<UIPanel>(item.uiElement))
		{
			// Handle panel-specific rendering
			UIColor panelColor = GetPanelColor(panel);
			materialToUse->SetDiffuseColor({ panelColor.r, panelColor.g, panelColor.b, panelColor.a });
		}

		BindMaterial(materialToUse);
		BindShaderForMaterial(materialToUse);


		// Setup UI transform buffer
		ConstantBuffer<TransfomBufferData> vsBuffer;
		vsBuffer.Initialize();
		vsBuffer.data.WVP = DirectX::XMMatrixTranspose(modelMatrix * s_UIProjectionMatrix);
		vsBuffer.data.Model = DirectX::XMMatrixTranspose(modelMatrix);
		vsBuffer.Update();

		RenderCommand::GetContext()->VSSetConstantBuffers(BindSlot::CB_Transform, 1, vsBuffer.GetAddressOf());

		// Setup UI-specific constant buffer
		ConstantBuffer<UIConstantBuffer> uiBuffer;
		uiBuffer.Initialize();
		uiBuffer.data.projection = DirectX::XMMatrixTranspose(s_UIProjectionMatrix);
		uiBuffer.data.screenWidth = static_cast<float>(RenderCommand::GetViewportWidth());
		uiBuffer.data.screenHeight = static_cast<float>(RenderCommand::GetViewportHeight());
		uiBuffer.data.time = static_cast<float>(s_FrameCount) / 60.0f; // Approximate time
		uiBuffer.data.padding = 0.0f;
		uiBuffer.Update();

		RenderCommand::GetContext()->VSSetConstantBuffers(BindSlot::CB_UI, 1, uiBuffer.GetAddressOf());

		// Use the UI quad mesh
		if (s_UIQuadModel && s_UIQuadModel->GetMesh())
		{
			auto mesh = s_UIQuadModel->GetMesh();
			mesh->BindBuffers();
			RenderCommand::DrawIndexed(mesh->GetIndexCount());
		}

		// Restore previous render state
		PopRenderState();

		// Update statistics
		s_Stats.drawCalls++;
		s_Stats.uiElements++;
	}


	void Renderer::SubmitUI(std::shared_ptr<UIElement> element)
	{
		if (!element || !element->IsVisible())
			return;

		DXEngine::RenderItem item(element, s_DefaultUIMaterial);
		ValidateRenderItem(item);
		s_RenderItems.push_back(item);
	}

	void Renderer::SubmitUIImmediate(std::shared_ptr<UIElement> element, std::shared_ptr<Material> material)
	{
		if (!element)
			return;

		// Save current render state
		PushRenderState();

		// Set UI render state
		SetUIRenderState();

		DXEngine::RenderItem item(element, material ? material : s_DefaultUIMaterial);
		ValidateRenderItem(item);
		RenderUIItem(item);

		// Restore previous render state
		PopRenderState();
	}

	void Renderer::SetRenderStateForQueue(RenderQueue queue)
	{
		s_Stats.renderStateChanges++;

		switch (queue)
		{
		case RenderQueue::Background:
			RenderCommand::SetRasterizerMode(RasterizerMode::SolidFrontCull);
			RenderCommand::SetDepthTestEnabled(true);
			RenderCommand::SetBlendEnabled(false);
			// Disable blending
			break;

		case RenderQueue::Opaque:
			RenderCommand::SetRasterizerMode(s_WireframeEnabled ?
				RasterizerMode::Wireframe : RasterizerMode::SolidBackCull);
			RenderCommand::SetDepthTestEnabled(true);
			// Disable blending, enable depth
			break;

		case RenderQueue::Transparent:
			RenderCommand::SetRasterizerMode(RasterizerMode::SolidBackCull);
			RenderCommand::SetBlendEnabled(true);
			// Enable alpha blending, disable depth writes
			break;

		case RenderQueue::UI:
			SetUIRenderState();
			break;

		case RenderQueue::Overlay:
			// Similar to UI but with different depth settings
			break;
		}
	}

	void Renderer::SetUIRenderState()
	{
		//set Orthographic projection
		//disable culling
		RenderCommand::SetRasterizerMode(RasterizerMode::SolidNoCull);
		RenderCommand::SetDepthTestEnabled(false);
		RenderCommand::SetBlendEnabled(true);
	}

	void Renderer::Set3DRenderState()
	{
		RenderCommand::SetDepthTestEnabled(true);
		// Disable blending (unless transparent)
		// Set perspective projection
		// Enable back-face culling

		RenderCommand::SetRasterizerMode(RasterizerMode::SolidBackCull);
	}

	void Renderer::PushRenderState()
	{
		RenderState state;
		state.depthEnabled = true; // Get from current state
		state.blendEnabled = false; // Get from current state
		state.rasterizerMode = RasterizerMode::SolidBackCull; // Get from current state
		state.projection = RenderCommand::GetCamera() ? RenderCommand::GetCamera()->GetProjection() : DirectX::XMMatrixIdentity();

		s_RenderStateStack.push_back(state);
	}

	void Renderer::PopRenderState()
	{
		if (!s_RenderStateStack.empty())
		{
			RenderState state = s_RenderStateStack.back();
			s_RenderStateStack.pop_back();

			// Restore state
			RenderCommand::SetDepthTestEnabled(state.depthEnabled);
			RenderCommand::SetBlendEnabled(state.blendEnabled);
			RenderCommand::SetRasterizerMode(state.rasterizerMode);
		}
	}

	void Renderer::CreateUIQuad()
	{
		// Create a unit quad mesh for UI rendering
		std::vector<Vertex> vertices = {
			Vertex(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f), // Top-left
			Vertex(1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f), // Top-right
			Vertex(1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f), // Bottom-right
			Vertex(0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f)  // Bottom-left
		};

		std::vector<unsigned short> indices = {
			0, 1, 2,  // First triangle
			2, 3, 0   // Second triangle
		};

		auto meshResource = std::make_shared<MeshResource>(std::move(vertices), std::move(indices));
		auto mesh = std::make_shared<Mesh>(meshResource);
		s_UIQuadModel = std::make_shared<Model>();
		s_UIQuadModel->SetMesh(mesh);
		
	}

	void Renderer::CreateDefaultUIMaterial()
	{
		s_DefaultUIMaterial = MaterialFactory::CreateUnlitMaterial("DefaultUI");
		s_DefaultUIMaterial->SetRenderQueue(RenderQueue::UI);
		s_DefaultUIMaterial->SetFlag(MaterialFlags::IsTransparent, true);
		s_DefaultUIMaterial->SetFlag(MaterialFlags::IsTwoSided, true);

		// Set a default white color for UI
		s_DefaultUIMaterial->SetDiffuseColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	}

	void Renderer::BindUIShader()
	{
	}

	void Renderer::UpdateUIProjectionMatrix(int width, int height)
	{
		// Create orthographic projection matrix for UI
		s_UIProjectionMatrix = DirectX::XMMatrixOrthographicOffCenterLH(
			0.0f,           // Left
			(float)width,   // Right
			(float)height,  // Bottom
			0.0f,           // Top
			0.0f,           // Near
			1.0f            // Far
		);
	}

	void Renderer::BindMaterial(std::shared_ptr<Material> material)
	{
		if (material != s_CurrentMaterial)
		{
			material->Bind();
			s_CurrentMaterial = material;
			s_Stats.materialsChanged++;
		}
	}
	void Renderer::BindShaderForMaterial(std::shared_ptr<Material> material)
	{
		if (!s_ShaderManager)
			return;

		MaterialType materialType = material->GetType();

		if (materialType != s_CurrentMaterialType)
		{
			auto shader = s_ShaderManager->GetShader(materialType);
			if (shader && shader != s_CurrentShader)
			{
				shader->Bind();
				s_CurrentShader = shader;
				s_CurrentMaterialType = materialType;
				s_Stats.shadersChanged++;
			}

		}

	}
	float Renderer::CalculateSortKey(const DXEngine::RenderItem& item)
	{
		if (!RenderCommand::GetCamera() || !item.model)
			return 0.0f;

		//calculate distance from the camera
		DirectX::XMVECTOR cameraPos = RenderCommand::GetCamera()->GetPos();
		DirectX::XMVECTOR modelPos = item.model->GetTranslation();
		DirectX::XMVECTOR distance = DirectX::XMVector3Length(DirectX::XMVectorSubtract(modelPos, cameraPos));

		return DirectX::XMVectorGetX(distance);
	}

	void Renderer::ValidateRenderItem(const DXEngine::RenderItem& item)
	{
		if (!item.model)
		{
			OutputDebugStringA("Warning: RenderItem has null model\n");
			return;
		}

		if (!item.model->GetMesh())
		{
			OutputDebugStringA("Warning: RenderItem model has null mesh\n");
			return;
		}

		if (!item.material)
		{
			OutputDebugStringA("Warning: RenderItem has null material\n");
			return;
		}

		if (!item.material->IsValid())
		{
			OutputDebugStringA(("Warning: Invalid material: " + item.material->GetName() + "\n").c_str());
		}
	}

	void Renderer::EnableDepthTesting(bool enable)
	{
		// Implementation depends on your depth state management
		// This would set depth testing on/off
	}

	void Renderer::EnableBlending(bool enable)
	{
		// Implementation depends on your blend state management
		// This would set alpha blending on/off
	}

	void Renderer::ResetStats()
	{
		memset(&s_Stats, 0, sizeof(RenderStatistics));
	}

	void Renderer::EnableWireframe(bool enable)
	{
		s_WireframeEnabled = enable;
	}

	void Renderer::EnableDebugInfo(bool enable)
	{
		s_DebugInfoEnabled = enable;
	}

	std::string Renderer::GetDebugInfo()
	{
		std::string info = "=== Render System Debug Info ===\n";
		info += "Frame: " + std::to_string(s_FrameCount) + "\n";
		info += "Draw Calls: " + std::to_string(s_Stats.drawCalls) + "\n";
		info += "Triangles: " + std::to_string(s_Stats.trianglesRendered) + "\n";
		info += "Material Changes: " + std::to_string(s_Stats.materialsChanged) + "\n";
		info += "Shader Changes: " + std::to_string(s_Stats.shadersChanged) + "\n";
		info += "Render Items: " + std::to_string(s_RenderItems.size()) + "\n";

		// Queue breakdown
		for (const auto& [queue, items] : s_SortedQueues)
		{
			info += "Queue " + std::to_string(static_cast<int>(queue)) +
				": " + std::to_string(items.size()) + " items\n";
		}

		info += "================================\n\n";
		return info;
	}

	void Renderer::OnWindowResize(int width, int height)
	{

		float aspect = static_cast<float>(width) / static_cast<float>(height);
		DirectX::XMMATRIX camProjection = DirectX::XMMatrixPerspectiveLH(1.0f, aspect, 0.5f, 100.0f);
		//RenderCommand::GetCamera().   //set the projection of the new camera TO-DO
		RenderCommand::Resize(width, height);
	}

	UIColor Renderer::GetButtonColorForState(std::shared_ptr<UIButton> button)
	{
		switch (button->GetState())
		{
		case ButtonState::Hovered:
			return button->GetHoverColor(); 
		case ButtonState::Pressed:
			return button->GetPressedColor();
		case ButtonState::Disabled:
			return UIColor(0.3f, 0.3f, 0.3f, 0.5f);
		default:
			return button->GetNormalColor();
		}
	}

	UIColor Renderer::GetPanelColor(std::shared_ptr<UIPanel> panel)
	{
		return panel->GetBackgroundColor(); 
	}
}