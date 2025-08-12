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

    // Static member definitions
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

        OutputDebugStringA("Renderer initialized successfully\n");
    }

    void Renderer::InitShaderManager(std::shared_ptr<ShaderManager> shaderManager)
    {
        s_ShaderManager = shaderManager;
        if (!s_ShaderManager)
        {
            OutputDebugStringA("Warning: Renderer initialized without ShaderManager!\n");
        }
        else
        {
            OutputDebugStringA("Renderer: ShaderManager initialized\n");
        }

        s_RenderItems.reserve(1000);
    }

    void Renderer::Shutdown()
    {
        OutputDebugStringA("Shutting down Renderer...\n");

        s_RenderItems.clear();
        s_SortedQueues.clear();
        s_ShaderManager.reset();
        s_CurrentMaterial.reset();
        s_CurrentShader.reset();
        s_UIQuadModel.reset();
        s_DefaultUIMaterial.reset();
        s_RenderStateStack.clear();

        RenderCommand::Shutdown();

        OutputDebugStringA("Renderer shutdown complete\n");
    }

    void Renderer::BeginScene(const std::shared_ptr<Camera>& camera)
    {
        if (!camera)
        {
            OutputDebugStringA("Warning: BeginScene called with null camera\n");
            return;
        }

        RenderCommand::SetCamera(camera);

        // Clear previous frame data
        s_RenderItems.clear();
        s_SortedQueues.clear();
        s_CurrentMaterial.reset();
        s_CurrentShader.reset();
        s_CurrentMaterialType = MaterialType::Unlit;

        ResetStats();
        s_FrameCount++;

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

    // 3D Model submission methods
    void Renderer::Submit(std::shared_ptr<Model> model)
    {
        if (!model || !model->GetMesh())
        {
            OutputDebugStringA("Warning: Attempting to submit invalid model\n");
            return;
        }

        auto mesh = model->GetMesh();
        if (!mesh->IsValid())
        {
            OutputDebugStringA("Warning: Model has invalid mesh\n");
            return;
        }

        // Submit each submesh as a separate render item
        size_t submeshCount = std::max(size_t(1), mesh->GetSubmeshCount());
        for (size_t i = 0; i < submeshCount; ++i)
        {
            auto material = mesh->GetMaterial(i);
            if (!material)
            {
                material = MaterialFactory::CreateLitMaterial("DefaultModelMaterial_" + std::to_string(i));
                mesh->SetMaterial(i, material);
            }

            DXEngine::RenderItem item(model, material, static_cast<uint32_t>(i));
            ValidateRenderItem(item);
            s_RenderItems.push_back(item);
        }
    }

    void Renderer::Submit(std::shared_ptr<Model> model, std::shared_ptr<Material> materialOverride)
    {
        if (!model || !model->GetMesh() || !materialOverride)
        {
            OutputDebugStringA("Warning: Invalid parameters for model submission with material override\n");
            return;
        }

        auto mesh = model->GetMesh();
        size_t submeshCount = std::max(size_t(1), mesh->GetSubmeshCount());

        for (size_t i = 0; i < submeshCount; ++i)
        {
            auto baseMaterial = mesh->GetMaterial(i);
            if (!baseMaterial)
            {
                baseMaterial = MaterialFactory::CreateLitMaterial("DefaultBaseMaterial");
            }

            DXEngine::RenderItem item(model, baseMaterial, static_cast<uint32_t>(i));
            item.materialOverride = materialOverride;
            item.queue = materialOverride->GetRenderQueue();

            ValidateRenderItem(item);
            s_RenderItems.push_back(item);
        }
    }

    void Renderer::Submit(std::shared_ptr<Model> model, uint32_t submeshIndex, std::shared_ptr<Material> material)
    {
        if (!model || !model->GetMesh())
        {
            OutputDebugStringA("Warning: Invalid model for submesh submission\n");
            return;
        }

        auto mesh = model->GetMesh();
        if (submeshIndex >= mesh->GetSubmeshCount() && mesh->GetSubmeshCount() > 0)
        {
            OutputDebugStringA("Warning: Submesh index out of range\n");
            return;
        }

        auto useMaterial = material ? material : mesh->GetMaterial(submeshIndex);
        if (!useMaterial)
        {
            useMaterial = MaterialFactory::CreateLitMaterial("DefaultSubmeshMaterial");
        }

        DXEngine::RenderItem item(model, useMaterial, submeshIndex);
        ValidateRenderItem(item);
        s_RenderItems.push_back(item);
    }

    void Renderer::SubmitImmediate(std::shared_ptr<Model> model, std::shared_ptr<Material> material)
    {
        if (!model || !model->GetMesh())
        {
            OutputDebugStringA("Warning: Invalid model for immediate rendering\n");
            return;
        }

        auto mesh = model->GetMesh();
        auto useMaterial = material ? material : mesh->GetMaterial(0);
        if (!useMaterial)
        {
            useMaterial = MaterialFactory::CreateLitMaterial("ImmediateMaterial");
        }

        DXEngine::RenderItem item(model, useMaterial);
        ValidateRenderItem(item);
        Render3DItem(item);
    }

    // UI submission methods
    void Renderer::SubmitUI(std::shared_ptr<UIElement> element)
    {
        if (!element || !element->IsVisible())
        {
            return;
        }

        DXEngine::RenderItem item(element, s_DefaultUIMaterial);
        ValidateRenderItem(item);
        s_RenderItems.push_back(item);
    }

    void Renderer::SubmitUI(std::shared_ptr<UIElement> element, std::shared_ptr<Material> materialOverride)
    {
        if (!element || !element->IsVisible())
        {
            return;
        }

        auto baseMaterial = s_DefaultUIMaterial;
        DXEngine::RenderItem item(element, baseMaterial);
        item.materialOverride = materialOverride;
        item.queue = materialOverride ? materialOverride->GetRenderQueue() : RenderQueue::UI;

        ValidateRenderItem(item);
        s_RenderItems.push_back(item);
    }

    void Renderer::SubmitUIImmediate(std::shared_ptr<UIElement> element, std::shared_ptr<Material> material)
    {
        if (!element)
        {
            return;
        }

        PushRenderState();
        SetUIRenderState();

        DXEngine::RenderItem item(element, material ? material : s_DefaultUIMaterial);
        ValidateRenderItem(item);
        RenderUIItem(item);

        PopRenderState();
    }

    void Renderer::ProcessRenderQueue()
    {
        if (s_RenderItems.empty())
            return;

        SortRenderItems();

        // Render each queue in order
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
            if (queueIt != s_SortedQueues.end() && !queueIt->second.empty())
            {
                SetRenderStateForQueue(queue);

                for (DXEngine::RenderItem* item : queueIt->second)
                {
                    if (item && item->IsValid())
                    {
                        RenderItem(*item);
                    }
                }
            }
        }

        // Restore default 3D render state
        Set3DRenderState();
    }

    void Renderer::SortRenderItems()
    {
        // Calculate sort keys and categorize items
        for (auto& item : s_RenderItems)
        {
            item.sortKey = CalculateSortKey(item);
            s_SortedQueues[item.queue].push_back(&item);
        }

        // Sort each queue appropriately
        for (auto& [queue, items] : s_SortedQueues)
        {
            switch (queue)
            {
            case RenderQueue::Background:
            case RenderQueue::Opaque:
                // Sort front to back for early Z rejection
                std::sort(items.begin(), items.end(),
                    [](const DXEngine::RenderItem* a, const DXEngine::RenderItem* b) {
                        return a->sortKey < b->sortKey;
                    });
                break;

            case RenderQueue::Transparent:
                // Sort back to front for proper alpha blending
                std::sort(items.begin(), items.end(),
                    [](const DXEngine::RenderItem* a, const DXEngine::RenderItem* b) {
                        return a->sortKey > b->sortKey;
                    });
                break;

            case RenderQueue::UI:
            case RenderQueue::Overlay:
                // UI elements can be sorted by depth/priority if needed
                // For now, maintain submission order
                break;
            }
        }
    }

    void Renderer::RenderItem(const DXEngine::RenderItem& item)
    {
        if (!item.IsValid())
        {
            OutputDebugStringA("Warning: Attempting to render invalid item\n");
            return;
        }

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

        RenderModel(item.model, materialToUse, item.submeshIndex);
    }

    void Renderer::RenderModel(std::shared_ptr<Model> model, std::shared_ptr<Material> material, uint32_t submeshIndex)
    {
        if (!model || !material)
            return;

        auto mesh = model->GetMesh();
        if (!mesh || !mesh->IsValid())
            return;

        RenderMesh(mesh, material, model->GetModelMatrix(), submeshIndex);
        s_Stats.modelsRendered++;
    }

    void Renderer::RenderMesh(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material,
        const DirectX::XMMATRIX& modelMatrix, uint32_t submeshIndex)
    {
        if (!mesh || !material)
            return;

        // Ensure GPU resources are ready
        if (!mesh->EnsureGPUResources())
        {
            OutputDebugStringA("Warning: Failed to ensure GPU resources for mesh\n");
            return;
        }

        // Bind shader for material
        BindShaderForMaterial(material);

        // Bind material properties
        BindMaterial(material);

        // Setup transform constant buffer
        ConstantBuffer<TransfomBufferData> vsBuffer;
        vsBuffer.Initialize();

        auto camera = RenderCommand::GetCamera();
        if (!camera)
        {
            OutputDebugStringA("Warning: No camera available for rendering\n");
            return;
        }

        auto view = camera->GetView();
        auto proj = camera->GetProjection();

        vsBuffer.data.WVP = DirectX::XMMatrixTranspose(modelMatrix * view * proj);
        vsBuffer.data.Model = DirectX::XMMatrixTranspose(modelMatrix);
        vsBuffer.Update();

        RenderCommand::GetContext()->VSSetConstantBuffers(BindSlot::CB_Transform, 1, vsBuffer.GetAddressOf());

        // Bind mesh buffers with shader bytecode for input layout
        if (s_CurrentShader)
        {
            // Get shader bytecode for input layout creation
            // This assumes ShaderProgram has a method to get vertex shader bytecode
            mesh->Bind(); // For now, bind without shader bytecode
        }
        else
        {
            mesh->Bind();
        }

        // Draw the specified submesh
        mesh->Draw(submeshIndex);

        // Update statistics
        s_Stats.drawCalls++;
        s_Stats.submeshesRendered++;

        // Estimate triangle count (this would need mesh resource data)
        auto meshResource = mesh->GetResource();
        if (meshResource && meshResource->GetIndexData())
        {
            s_Stats.trianglesRendered += meshResource->GetIndexData()->GetIndexCount() / 3;
        }
    }

    void Renderer::RenderUIItem(const DXEngine::RenderItem& item)
    {
        if (!item.uiElement || !s_UIQuadModel)
            return;

        // Save current render state
        PushRenderState();
        SetUIRenderState();

        // Get UI element bounds and properties
        const UIRect& bounds = item.uiElement->GetBounds();

        // Create transformation matrix for this UI element
        DirectX::XMMATRIX scaleMatrix = DirectX::XMMatrixScaling(bounds.width, bounds.height, 1.0f);
        DirectX::XMMATRIX translationMatrix = DirectX::XMMatrixTranslation(bounds.x, bounds.y, 0.0f);
        DirectX::XMMATRIX modelMatrix = scaleMatrix * translationMatrix;

        // Determine material to use
        auto materialToUse = item.materialOverride ? item.materialOverride : s_DefaultUIMaterial;

        // Handle element-specific properties
        if (auto button = std::dynamic_pointer_cast<UIButton>(item.uiElement))
        {
            UIColor buttonColor = GetButtonColorForState(button);
            // Clone material or create dynamic material for color changes
            materialToUse->SetDiffuseColor({ buttonColor.r, buttonColor.g, buttonColor.b, buttonColor.a });
        }
        else if (auto panel = std::dynamic_pointer_cast<UIPanel>(item.uiElement))
        {
            UIColor panelColor = GetPanelColor(panel);
            materialToUse->SetDiffuseColor({ panelColor.r, panelColor.g, panelColor.b, panelColor.a });
        }

        // Bind material and shader
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
        uiBuffer.data.time = static_cast<float>(s_FrameCount) / 60.0f;
        uiBuffer.data.padding = 0.0f;
        uiBuffer.Update();

        RenderCommand::GetContext()->VSSetConstantBuffers(BindSlot::CB_UI, 1, uiBuffer.GetAddressOf());

        // Render the UI quad
        auto mesh = s_UIQuadModel->GetMesh();
        if (mesh && mesh->IsValid())
        {
            mesh->EnsureGPUResources();
            mesh->Bind();
            mesh->Draw();
        }

        // Restore previous render state
        PopRenderState();

        // Update statistics
        s_Stats.drawCalls++;
        s_Stats.uiElements++;
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
            break;

        case RenderQueue::Opaque:
            RenderCommand::SetRasterizerMode(s_WireframeEnabled ?
                RasterizerMode::Wireframe : RasterizerMode::SolidBackCull);
            RenderCommand::SetDepthTestEnabled(true);
            RenderCommand::SetBlendEnabled(false);
            break;

        case RenderQueue::Transparent:
            RenderCommand::SetRasterizerMode(RasterizerMode::SolidBackCull);
            RenderCommand::SetDepthTestEnabled(true);  // Read depth but don't write
            RenderCommand::SetBlendEnabled(true);
            break;

        case RenderQueue::UI:
            SetUIRenderState();
            break;

        case RenderQueue::Overlay:
            RenderCommand::SetRasterizerMode(RasterizerMode::SolidNoCull);
            RenderCommand::SetDepthTestEnabled(false);
            RenderCommand::SetBlendEnabled(true);
            break;
        }
    }

    void Renderer::SetUIRenderState()
    {
        RenderCommand::SetRasterizerMode(RasterizerMode::SolidNoCull);
        RenderCommand::SetDepthTestEnabled(false);
        RenderCommand::SetBlendEnabled(true);
    }

    void Renderer::Set3DRenderState()
    {
        RenderCommand::SetRasterizerMode(RasterizerMode::SolidBackCull);
        RenderCommand::SetDepthTestEnabled(true);
        RenderCommand::SetBlendEnabled(false);
    }

    void Renderer::PushRenderState()
    {
        RenderState state;
        // In a real implementation, you'd query the current state from the render command
        state.depthEnabled = true;
        state.blendEnabled = false;
        state.rasterizerMode = RasterizerMode::SolidBackCull;
        state.projection = RenderCommand::GetCamera() ?
            RenderCommand::GetCamera()->GetProjection() : DirectX::XMMatrixIdentity();

        s_RenderStateStack.push_back(state);
    }

    void Renderer::PopRenderState()
    {
        if (!s_RenderStateStack.empty())
        {
            RenderState state = s_RenderStateStack.back();
            s_RenderStateStack.pop_back();

            RenderCommand::SetDepthTestEnabled(state.depthEnabled);
            RenderCommand::SetBlendEnabled(state.blendEnabled);
            RenderCommand::SetRasterizerMode(state.rasterizerMode);
        }
    }

    void Renderer::CreateUIQuad()
    {
        // Create a unit quad mesh for UI rendering (0,0 to 1,1)
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

        try
        {
            auto meshResource = std::make_shared<MeshResource>(std::move(vertices), std::move(indices));
            auto mesh = std::make_shared<Mesh>(meshResource);
            s_UIQuadModel = std::make_shared<Model>();
            s_UIQuadModel->SetMesh(mesh);

            OutputDebugStringA("UI Quad created successfully\n");
        }
        catch (const std::exception& e)
        {
            OutputDebugStringA(("Failed to create UI Quad: " + std::string(e.what()) + "\n").c_str());
        }
    }

    void Renderer::CreateDefaultUIMaterial()
    {
        try
        {
            s_DefaultUIMaterial = MaterialFactory::CreateUnlitMaterial("DefaultUI");
            s_DefaultUIMaterial->SetRenderQueue(RenderQueue::UI);
            s_DefaultUIMaterial->SetFlag(MaterialFlags::IsTransparent, true);
            s_DefaultUIMaterial->SetFlag(MaterialFlags::IsTwoSided, true);
            s_DefaultUIMaterial->SetDiffuseColor({ 1.0f, 1.0f, 1.0f, 1.0f });

            OutputDebugStringA("Default UI Material created successfully\n");
        }
        catch (const std::exception& e)
        {
            OutputDebugStringA(("Failed to create Default UI Material: " + std::string(e.what()) + "\n").c_str());
        }
    }

    void Renderer::UpdateUIProjectionMatrix(int width, int height)
    {
        // Create orthographic projection matrix for UI (0,0 at top-left)
        s_UIProjectionMatrix = DirectX::XMMatrixOrthographicOffCenterLH(
            0.0f,           // Left
            (float)width,   // Right  
            (float)height,  // Bottom
            0.0f,           // Top
            0.0f,           // Near
            1.0f            // Far
        );

        OutputDebugStringA(("UI Projection matrix updated for " + std::to_string(width) + "x" + std::to_string(height) + "\n").c_str());
    }

    void Renderer::BindMaterial(std::shared_ptr<Material> material)
    {
        if (!material)
        {
            OutputDebugStringA("Warning: Attempting to bind null material\n");
            return;
        }

        if (material != s_CurrentMaterial)
        {
            material->Bind();
            s_CurrentMaterial = material;
            s_Stats.materialsChanged++;
        }
    }

    void Renderer::BindShaderForMaterial(std::shared_ptr<Material> material)
    {
        if (!s_ShaderManager || !material)
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
            else if (!shader)
            {
                OutputDebugStringA(("Warning: No shader available for material type " +
                    std::to_string(static_cast<int>(materialType)) + "\n").c_str());
            }
        }
    }

    float Renderer::CalculateSortKey(const DXEngine::RenderItem& item)
    {
        auto camera = RenderCommand::GetCamera();
        if (!camera)
            return 0.0f;

        DirectX::XMVECTOR cameraPos = camera->GetPos();

        if (item.isUIElement)
        {
            // For UI, we can use Z-order or element priority
            // For now, return 0 to maintain submission order
            return 0.0f;
        }
        else if (item.model)
        {
            // Calculate distance from camera to model
            DirectX::XMVECTOR modelPos = item.model->GetTranslation();
            DirectX::XMVECTOR distance = DirectX::XMVector3Length(DirectX::XMVectorSubtract(modelPos, cameraPos));
            return DirectX::XMVectorGetX(distance);
        }

        return 0.0f;
    }

    void Renderer::ValidateRenderItem(const DXEngine::RenderItem& item)
    {
        if (!item.IsValid())
        {
            OutputDebugStringA("Error: Invalid render item detected\n");
            return;
        }

        if (item.isUIElement)
        {
            if (!item.uiElement)
            {
                OutputDebugStringA("Warning: UI RenderItem has null UIElement\n");
            }
        }
        else
        {
            if (!item.model)
            {
                OutputDebugStringA("Warning: 3D RenderItem has null model\n");
                return;
            }

            if (!item.model->GetMesh())
            {
                OutputDebugStringA("Warning: RenderItem model has null mesh\n");
                return;
            }

            if (!item.model->GetMesh()->IsValid())
            {
                OutputDebugStringA("Warning: RenderItem has invalid mesh\n");
                return;
            }
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

    void Renderer::OnWindowResize(int width, int height)
    {
        OutputDebugStringA(("Window resized to " + std::to_string(width) + "x" + std::to_string(height) + "\n").c_str());

        // Update render command viewport
        RenderCommand::Resize(width, height);

        // Update UI projection matrix
        UpdateUIProjectionMatrix(width, height);

        // Update camera aspect ratio if camera exists
        auto camera = RenderCommand::GetCamera();
        if (camera)
        {
            float aspect = static_cast<float>(width) / static_cast<float>(height);
            // Note: You'd need to add SetAspectRatio method to your Camera class
            // camera->SetAspectRatio(aspect);
        }
    }

    UIColor Renderer::GetButtonColorForState(std::shared_ptr<UIButton> button)
    {
        if (!button)
            return UIColor(1.0f, 1.0f, 1.0f, 1.0f);

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
        if (!panel)
            return UIColor(0.5f, 0.5f, 0.5f, 1.0f);

        return panel->GetBackgroundColor();
    }

    void Renderer::ResetStats()
    {
        memset(&s_Stats, 0, sizeof(RenderStatistics));
    }

    void Renderer::EnableWireframe(bool enable)
    {
        s_WireframeEnabled = enable;
        OutputDebugStringA(enable ? "Wireframe mode enabled\n" : "Wireframe mode disabled\n");
    }

    void Renderer::EnableDebugInfo(bool enable)
    {
        s_DebugInfoEnabled = enable;
        OutputDebugStringA(enable ? "Debug info enabled\n" : "Debug info disabled\n");
    }

    std::string Renderer::GetDebugInfo()
    {
        std::string info = "=== Render System Debug Info ===\n";
        info += "Frame: " + std::to_string(s_FrameCount) + "\n";
        info += "Draw Calls: " + std::to_string(s_Stats.drawCalls) + "\n";
        info += "Models Rendered: " + std::to_string(s_Stats.modelsRendered) + "\n";
        info += "Submeshes Rendered: " + std::to_string(s_Stats.submeshesRendered) + "\n";
        info += "UI Elements: " + std::to_string(s_Stats.uiElements) + "\n";
        info += "Triangles: " + std::to_string(s_Stats.trianglesRendered) + "\n";
        info += "Material Changes: " + std::to_string(s_Stats.materialsChanged) + "\n";
        info += "Shader Changes: " + std::to_string(s_Stats.shadersChanged) + "\n";
        info += "Render State Changes: " + std::to_string(s_Stats.renderStateChanges) + "\n";
        info += "Total Render Items: " + std::to_string(s_RenderItems.size()) + "\n";

        // Queue breakdown
        for (const auto& [queue, items] : s_SortedQueues)
        {
            std::string queueName;
            switch (queue)
            {
            case RenderQueue::Background: queueName = "Background"; break;
            case RenderQueue::Opaque: queueName = "Opaque"; break;
            case RenderQueue::Transparent: queueName = "Transparent"; break;
            case RenderQueue::UI: queueName = "UI"; break;
            case RenderQueue::Overlay: queueName = "Overlay"; break;
            default: queueName = "Unknown"; break;
            }

            info += queueName + " Queue: " + std::to_string(items.size()) + " items\n";
        }

        // Memory and performance info
        if (s_ShaderManager)
        {
            info += "\n" + s_ShaderManager->GetShaderInfo() + "\n";
        }

        info += "================================\n\n";
        return info;
    }
}