#include "dxpch.h"
#include "Renderer.h"
#include "models/Model.h"
#include "utils/Mesh/Mesh.h"
#include "camera/Camera.h"
#include "shaders/ShaderManager.h"
#include <algorithm>
#include "utils/UI/UIElement.h"
#include <utils/UI/UIButton.h>
#include <utils/UI/UIPanel.h>
#include <utils/UI/UIText.h>
#include <DirectXCollision.h>
#include "utils/Light.h"
#include "utils/Sampler.h"


namespace DXEngine {

    // Static member definitions
    Renderer::RenderStatistics Renderer::s_Stats;
    std::shared_ptr<ShaderManager> Renderer::s_ShaderManager = nullptr;
    std::vector<RenderSubmission> Renderer::s_RenderSubmissions;
    const DXEngine::RenderSubmission* Renderer::s_CurrentRenderSubmission = nullptr;
    std::map<RenderQueue, std::vector<RenderSubmission*>> Renderer::s_SortedQueues;
    std::vector<RenderBatch> Renderer::s_RenderBatches;
    std::shared_ptr<Material> Renderer::s_CurrentMaterial = nullptr;
    std::shared_ptr<ShaderProgram> Renderer::s_CurrentShader = nullptr;
    MaterialType Renderer::s_CurrentMaterialType = MaterialType::Unlit;
    std::shared_ptr<TransfomBufferData> Renderer::s_TransformBufferData = nullptr;

    std::shared_ptr<Model> Renderer::s_UIQuadModel = nullptr;
    std::shared_ptr<Material> Renderer::s_DefaultUIMaterial = nullptr;
    DirectX::XMMATRIX Renderer::s_UIProjectionMatrix = DirectX::XMMatrixIdentity();
    std::shared_ptr<UIConstantBuffer> Renderer::s_UIBufferData = nullptr;
    std::vector<Renderer::RenderState> Renderer::s_RenderStateStack;

    std::shared_ptr<LightManager> Renderer::s_LightManager = nullptr;


    bool Renderer::s_WireframeEnabled = false;
    bool Renderer::sDX_DEBUGInfoEnabled = false;
    bool Renderer::s_InstanceEnabled = true;
    bool Renderer::s_FrustumCullingEnabled = true;
    size_t Renderer::s_InstanceBatchSize = 100;
    uint32_t Renderer::s_FrameCount = 0;
    float Renderer::s_Time = 0.0f;

    RenderSubmission RenderSubmission::CreateFromModel(const Model* model, size_t meshIndex, size_t submeshIndex)
    {
        RenderSubmission submission;

        if (!model || !model->IsValid())
            return submission;

        submission.sourceModel = model;
        submission.meshIndex = meshIndex;
        submission.submeshIndex = submeshIndex;
        submission.mesh = model->GetMesh(meshIndex);
        
        if (submission.mesh)
        {
            submission.material = submission.mesh->GetMaterial(submeshIndex);
            if (submission.material)
            {
                submission.queue = submission.material->GetRenderQueue();
            }
        }

        // Store transform
        DirectX::XMMATRIX  modelMatrix = model->GetModelMatrix();
        DirectX::XMStoreFloat4x4(&submission.modelMatrix, modelMatrix);

        // Calculate normal matrix
        DirectX::XMMATRIX normalMatrix = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, modelMatrix));
        DirectX::XMStoreFloat4x4(&submission.normalMatrix, normalMatrix);

        submission.visible = model->IsVisible();
        submission.castsShadow = model->CastsShadows();
        submission.receivesShadows = model->ReceivesShadows();

        if (model->IsInstanced())
        {
            const InstanceData* instanceData = model->GetInstanceData();
            if (instanceData && instanceData->GetInstanceCount() > 0)
            {
                submission.instanceTransforms = &instanceData->transforms;
                submission.instanceCount = instanceData->GetInstanceCount();
            }
        }

        if (model->IsSkinned())
        {
            const SkinningData* skinData = model->GetSkinningData();
            if (skinData && !skinData->boneMatrices.empty())
            {
                submission.boneMatrices = &skinData->boneMatrices;
            }
        }
        
        return submission;
    }

    RenderSubmission RenderSubmission::CreateFromUIElement(std::shared_ptr<UIElement> element, std::shared_ptr<Material> material)
    {
        RenderSubmission submission;
        submission.uiElement = element;
        submission.material = material;
        submission.queue = RenderQueue::UI;
        submission.isUIElement = true;
        submission.visible = element ? element->IsVisible() : false;

        DirectX::XMStoreFloat4x4(&submission.modelMatrix, DirectX::XMMatrixIdentity());
        DirectX::XMStoreFloat4x4(&submission.normalMatrix, DirectX::XMMatrixIdentity());

        return submission;
    }
    
    bool RenderSubmission::IsValid()const
    {
        if (isUIElement)
        {
            return uiElement != nullptr && material != nullptr;
        }
        else
        {
            return mesh != nullptr && mesh->IsValid() && material != nullptr && sourceModel != nullptr;
        }
    }

    void Renderer::Init(HWND hwnd, int width, int height)
    {
        RenderCommand::Init(hwnd, width, height);
        //Sampler
        SamplerManager::Instance().Initialize();

        // Initialize ShaderManager
        s_ShaderManager = std::make_shared<ShaderManager>();
        s_ShaderManager->Initialize();
        s_ShaderManager->EnableDynamicVariants(true);

        CreateUIQuad();
        CreateDefaultUIMaterial();
        UpdateUIProjectionMatrix(width, height);

        s_RenderSubmissions.reserve(1000);
        s_RenderBatches.reserve(100);

        ResetStats();

        OutputDebugStringA("Renderer initialized successfully\n");
    }


    void Renderer::InitLightManager()
    {
        s_LightManager = std::make_shared<LightManager>();

        //create default lighting setup

       auto sunLight = s_LightManager->CreateDirectionalLight();
       if (sunLight)
       {
           sunLight->SetDirection({ 0.3f,-0.8,0.2 });
           sunLight->SetColor({ 1.0f, 0.95f, 0.8f });
           sunLight->SetIntensity(2.0f);
           sunLight->SetCastShadows(false);
       }
       // Set default ambient
        s_LightManager->SetAmbientLight({ 0.2f, 0.25f, 0.3f }, 0.1f);
        s_LightManager->SetExposure(0.4f);

       // auto point = s_LightManager->CreatePointLight();
       // if (point)
       // {
       //     point->SetColor({ 1.0f, 1.0f, 1.0f });
       //     point->SetIntensity(200.0f);
       //     point->SetRadius(100.f);
       //     point->SetPosition({ 0.0f, 15.0f, 4.0f });
       //     point->SetAttenuation({ 50.0f, 0.0f, 2.0f });
       //     point->SetCastShadows(false);
       // }


    }

    void Renderer::Shutdown()
    {
        OutputDebugStringA("Shutting down Renderer...\n");

        s_RenderSubmissions.clear();
        s_SortedQueues.clear();
        s_RenderBatches.clear();
        s_ShaderManager.reset();
        s_LightManager.reset(); 
        s_CurrentMaterial.reset();
        s_CurrentShader.reset();
        s_UIQuadModel.reset();
        s_DefaultUIMaterial.reset();
        s_RenderStateStack.clear();

        SamplerManager::Instance().Shutdown();

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

        UpdateLightCulling(camera);

        // Clear previous frame data
        s_RenderSubmissions.clear();
        s_SortedQueues.clear();
        s_RenderBatches.clear();
        s_CurrentMaterial.reset();
        s_CurrentShader.reset();
        s_CurrentMaterialType = MaterialType::Unlit;

        ResetStats();
        s_FrameCount++;

        RenderCommand::Clear();

        // Check for shader hot reload in debug builds
#ifdef DX_DEBUG
        if (s_ShaderManager)
        {
            s_ShaderManager->Update();
        }
#endif
    }

    void Renderer::EndScene()
    {
        ProcessRenderQueue();
        RenderCommand::Present();

        if (sDX_DEBUGInfoEnabled)
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
        if (!model || !model->IsValid()||!model->IsVisible())
            return;

        s_Stats.modelsSubmitted++;
        ProcessModelSubmission(model);
    }

    void Renderer::Submit(std::shared_ptr<Model> model, std::shared_ptr<Material> materialOverride)
    {
        if (!model || !model->IsValid()||!model->IsVisible())
            return;
        s_Stats.modelsSubmitted++;
        ProcessModelSubmission(model, materialOverride);
  
    }

    void Renderer::SubmitMesh(std::shared_ptr<Model> model, size_t meshIndex, std::shared_ptr<Material> materialOverride)
    {
        if (!model || !model->IsValid() || !model->IsVisible())
            return;

        if (meshIndex >= model->GetMeshCount())
        return;

        auto mesh = model->GetMesh(meshIndex);
        if (!mesh || !mesh->IsValid())
            return;

        //create submission for this specific mesh
        size_t submeshCount = std::max(size_t(1), mesh->GetSubmeshCount());
        for (size_t submeshIndex = 0; submeshIndex < submeshCount; ++submeshIndex)
        {
            DXEngine::RenderSubmission submission = DXEngine::RenderSubmission::CreateFromModel(model.get(), meshIndex, submeshIndex);

            if (materialOverride)
            {
                submission.materialOverride = materialOverride;
                submission.queue = materialOverride->GetRenderQueue();
            }
            if (submission.IsValid())
            {
                ValidateSubmission(submission);
                s_RenderSubmissions.push_back(submission);
                s_Stats.submissionProcessed++;
            }

        }

    }

    void Renderer::SubmitSubmesh(std::shared_ptr<Model> model, size_t meshIndex, size_t submeshIndex, std::shared_ptr<Material> materialOverride)
    {
        if (!model || !model->IsValid() || !model->IsVisible())
            return;

        if (meshIndex >= model->GetMeshCount())
            return;

        auto mesh = model->GetMesh(meshIndex);
        if (!mesh || !mesh->IsValid())
            return;

        if (submeshIndex >= std::max(size_t(1), mesh->GetSubmeshCount()))
            return;

        DXEngine::RenderSubmission submission = RenderSubmission::CreateFromModel(model.get(), meshIndex, submeshIndex);

        if (materialOverride)
        {
            submission.materialOverride = materialOverride;
            submission.queue = materialOverride->GetRenderQueue();
        }
        
        if (submission.IsValid())
        {
            ValidateSubmission(submission);
            s_RenderSubmissions.push_back(submission);
            s_Stats.submissionProcessed++;
        }

    }

    void Renderer::RenderImmediate(std::shared_ptr<Model> model, std::shared_ptr<Material> materialOverride)
    {
        if (!model || !model->IsValid())
            return;

        model->EnsureDefaultMaterials();

        //Render
        for (size_t meshIndex = 0; meshIndex < model->GetMeshCount(); ++meshIndex)
        {
            RenderMeshImmediate(model, meshIndex, materialOverride);
        }
    }

    void Renderer::RenderMeshImmediate(std::shared_ptr<Model> model, size_t meshIndex, std::shared_ptr<Material> materialOverride)
    {
        if (!model || !model->IsValid())
            return;

        if (meshIndex >= model->GetMeshCount())
            return;

        auto mesh = model->GetMesh(meshIndex);
        if (!mesh || !mesh->IsValid())
            return;

        size_t submeshCount = std::max(size_t(1), mesh->GetSubmeshCount());
        for (size_t submeshIndex = 0; submeshIndex < submeshCount; ++submeshIndex)
        {
            DXEngine::RenderSubmission submission = RenderSubmission::CreateFromModel(model.get());

            if (materialOverride)
            {
                submission.materialOverride = materialOverride;
            }

            if (submission.IsValid())
            {
                RenderSubmission(submission);
            }
        }
    }

    //UI rendering
    void Renderer::SubmitUI(std::shared_ptr<UIElement> element)
    {
        if (!element || !element->IsVisible())
            return;

        DXEngine::RenderSubmission submission = DXEngine::RenderSubmission::CreateFromUIElement(element, s_DefaultUIMaterial);

        if (submission.IsValid())
        {
            ValidateSubmission(submission);
            s_RenderSubmissions.push_back(submission);
            s_Stats.submissionProcessed++;
        }
    }

    void Renderer::SubmitUI(std::shared_ptr<UIElement> element, std::shared_ptr<Material> materialOverride)
    {
        if (!element || !element->IsVisible())
            return;

        DXEngine::RenderSubmission submission = DXEngine::RenderSubmission::CreateFromUIElement(element, materialOverride);
        submission.materialOverride = materialOverride;

        if (materialOverride)
        {
            submission.queue = materialOverride->GetRenderQueue();
        }

        if (submission.IsValid())
        {
            ValidateSubmission(submission);
            s_RenderSubmissions.push_back(submission);
            s_Stats.submissionProcessed++;
        }
    }

    void Renderer::RenderUIImmediate(std::shared_ptr<UIElement> element, std::shared_ptr<Material> material)
    {
        if (!element || !element->IsVisible())
            return;

        PushRenderState();
        SetUIRenderState();

        DXEngine::RenderSubmission submission = DXEngine::RenderSubmission::CreateFromUIElement(element, material);

        if (submission.IsValid())
        {
            RenderUIElement(submission);
        }

        PopRenderState();
    }

    //core Rendering Pipeline
    void Renderer::ProcessRenderQueue()
    {
        if (s_RenderSubmissions.empty())
        { 
            OutputDebugStringA("Process RenderQueue() failed");
            return;
         }

        // Bind light data before rendering
        if (s_LightManager)
        {
            s_LightManager->BindLightData();
        }

        //sort submission and create batches
        SortSubmissions();
        CreateRenderBatches();

        //proces render batches in queue order
        std::vector<RenderQueue> queueOrder =
        {
            RenderQueue::Background,
            RenderQueue::Opaque,
            RenderQueue::Transparent,
            RenderQueue::UI,
            RenderQueue::Overlay
        };

        for (RenderQueue queue : queueOrder)
        {
            for (const auto& batch : s_RenderBatches)
            {
                if (batch.queue == queue && !batch.IsEmpty())
                {
                    SetRenderStateForQueue(queue);
                    ProcessRenderBatch(batch);
                }
            }
        }

        //restore state
        Set3DRenderState();

    }

    void Renderer::SortSubmissions()
    {
        //calculate sort keys for all submissions
        for (auto& submission : s_RenderSubmissions)
        {
            submission.sortKey = CalculateDistancetoCamera(submission);
            submission.batchKey = GenarateBatchKey(submission);
            s_SortedQueues[submission.queue].push_back(&submission);
        }
        //sort each queue appropriately
        for (auto& [queue, submissions] : s_SortedQueues)
        {
            switch (queue)
            {
            case RenderQueue::Background:
            case RenderQueue::Opaque:
            {
                // Sort front to back for early Z rejection
                std::sort(submissions.begin(), submissions.end(), [](const DXEngine::RenderSubmission* a, const DXEngine::RenderSubmission* b)
                    { return a->sortKey < b->sortKey; });
                break;
            }
            case RenderQueue::Transparent:
            {
                // Sort back to front for proper alpha blending
                std::sort(submissions.begin(), submissions.end(), [](const DXEngine::RenderSubmission* a, const DXEngine::RenderSubmission* b)
                    { return a->sortKey > b->sortKey; });
                break;
            }
            case RenderQueue::UI:
            case RenderQueue::Overlay:
                break;
            }
        }
    
    }

    void Renderer::CreateRenderBatches()
    {
        s_RenderBatches.clear();

        for (auto& [queue, submissions] : s_SortedQueues)
        {
            if (submissions.empty())
                return;

            //for UI and Overlay ques . create simple batches 
            
            if (queue == RenderQueue::UI || queue == RenderQueue::Overlay)
            {
                RenderBatch batch;
                batch.queue = queue;

                for (auto* submission : submissions)
                {
                    batch.submissions.push_back(*submission);
                }
                s_RenderBatches.push_back(batch);
                continue;
            }

            //for 3d queues,, try to batch by material and mesh
            DXEngine::RenderBatch currentBatch;
            currentBatch.queue = queue;

            for (auto* submission : submissions)
            {
                bool canBatch = false;

                if (!currentBatch.IsEmpty())
                {
                    const auto& lastSubmission = currentBatch.submissions.back();

                    //check if we can batch with the previous submisision
                    if (submission->mesh == lastSubmission.mesh &&
                        submission->GetEffectiveMaterial() == lastSubmission.GetEffectiveMaterial() &&
                        submission->instanceTransforms == nullptr &&
                        lastSubmission.instanceTransforms == nullptr)
                    {
                        canBatch = true;
                    }
                }

                if (canBatch && currentBatch.submissions.size() < s_InstanceBatchSize)
                {
                    currentBatch.submissions.push_back(*submission);
                }
                else
                {
                    //finish current batch and start new one 
                    if (!currentBatch.IsEmpty())
                    {
                        s_RenderBatches.push_back(currentBatch);
                        s_Stats.batchesProcessed++;
                    }

                    currentBatch.Clear();
                    currentBatch.queue = queue;
                    currentBatch.submissions.push_back(*submission);

                    //set batch properties based on first submission
                    currentBatch.batchMaterial = submission->GetEffectiveMaterial();
                    currentBatch.batchMesh = submission->mesh;
                    currentBatch.IsInstanced = (submission->instanceTransforms != nullptr);
                }
            }
            //add final batch
            if (!currentBatch.IsEmpty())
            {
                s_RenderBatches.push_back(currentBatch);
                s_Stats.batchesProcessed++;
            }
        }
    }

    void Renderer::ProcessRenderBatch(const DXEngine::RenderBatch& batch)
    {
        if (batch.IsEmpty())
            return;

        for (const auto& submission : batch.submissions)
        {
            RenderSubmission(submission);
        }
    }

    //Submission processing
    void Renderer::ProcessModelSubmission(std::shared_ptr<Model> model, std::shared_ptr<Material> materialOverride)
    {
        if (!model || !model->IsValid())
        {
            OutputDebugStringA("Warning: Attempting to submit invalid model\n");
            return;
        }


        model->EnsureDefaultMaterials();

        //frustum culling check
        if (s_FrustumCullingEnabled && !IsModelVisible(model.get(), RenderCommand::GetCamera()))
        {
            return;
        }

        //submit all meshes in that model
        for (size_t meshIndex = 0; meshIndex < model->GetMeshCount(); ++meshIndex)
        {
            auto mesh = model->GetMesh(meshIndex);
            if (!mesh || !mesh->IsValid())
                continue;

            //submit all the submeshes
            size_t submeshCount = std::max(size_t(1), mesh->GetSubmeshCount());
            for (size_t submeshIndex = 0; submeshIndex < submeshCount; ++submeshIndex)
            {
                DXEngine::RenderSubmission submission = DXEngine::RenderSubmission::CreateFromModel(model.get(), meshIndex, submeshIndex);

                if (materialOverride)
                {
                    submission.materialOverride = materialOverride;
                    submission.queue = materialOverride->GetRenderQueue();
                }
                if (submission.IsValid())
                {
                    ValidateSubmission(submission);
                    s_RenderSubmissions.push_back(submission);
                    s_Stats.submissionProcessed++;

                    //update stats based on features
                    if (model->IsInstanced())
                    {
                        s_Stats.instancesRendered += static_cast<uint32_t>(model->GetInstanceCount());
                    }
                }
            }
        }

    }

    //culling and LOD
    bool Renderer::IsModelVisible(const Model* model, const std::shared_ptr<Camera>& camera)
    {
        if (!model || !camera)
            return true; // If no model or camera, assume visible to prevent accidental culling

        // Get the model's world bounding sphere
        BoundingSphere worldSphere = model->GetWorldBoundingSphere();

        // Create DirectX bounding sphere
        DirectX::BoundingSphere dxWorldSphere(
            DirectX::XMFLOAT3(worldSphere.center.x, worldSphere.center.y, worldSphere.center.z),
            worldSphere.radius
        );

        // Get view and projection matrices
        DirectX::XMMATRIX view = camera->GetView();
        DirectX::XMMATRIX proj = camera->GetProjection();

        // Create frustum in projection space first
        DirectX::BoundingFrustum frustum(proj);

        // Transform frustum to world space using inverse view matrix
        DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(nullptr, view);
        DirectX::BoundingFrustum worldSpaceFrustum;
        frustum.Transform(worldSpaceFrustum, invView);

        // Add a small bias to the sphere radius to prevent edge cases
        dxWorldSphere.Radius *= 1.05f; // 5% larger radius for safety (reduced from 10%)

        // Check containment
        DirectX::ContainmentType containment = worldSpaceFrustum.Contains(dxWorldSphere);

        // Log culling decisions in debug mode
#ifdef DX_DEBUG
      //  if (containment == DirectX::DISJOINT) {
      //      std::string debugMsg = "Model culled - Center: (" +
      //          std::to_string(worldSphere.center.x) + ", " +
      //          std::to_string(worldSphere.center.y) + ", " +
      //          std::to_string(worldSphere.center.z) + ") Radius: " +
      //          std::to_string(worldSphere.radius) + "\n";
      //      OutputDebugStringA(debugMsg.c_str());
      //  }
#endif

        // Consider both CONTAINS and INTERSECTS as visible
        return containment != DirectX::DISJOINT;
    }

    size_t Renderer::SelectLODLevel(const Model* model, const std::shared_ptr<Camera>& camera)
    {
        if (!model || !camera)
            return 0;

        //simple distance-based LOD selection
        DirectX::XMVECTOR cameraPos = camera->GetPos();
        DirectX::XMVECTOR modelPos = model->GetTranslation();
        DirectX::XMVECTOR distance = DirectX::XMVector3Length(DirectX::XMVectorSubtract(modelPos, cameraPos));

        float distanceValue = DirectX::XMVectorGetX(distance);

        //LOD Threshholds (configurable)
        if (distanceValue < 50.0f)return 0;      //high detail
        else if (distanceValue < 150.0f)return 1;//medium detail
        else return 2;                           //low Detail

    }

    ///Rendering methods
    void Renderer::RenderSubmission(const DXEngine::RenderSubmission& submission)
    {
        if (!submission.IsValid()) {
            OutputDebugStringA("Warning: Attempting to render invalid submission\n");
            return;
        }

        // Track current submission for shader variant selection
        s_CurrentRenderSubmission = &submission;

        if (submission.isUIElement) {
            RenderUIElement(submission);
        }
        else if (submission.instanceTransforms && submission.instanceCount > 0) {
            RenderInstanceMesh(submission);
        }
        else if (submission.boneMatrices) {
            RenderSkinnedMesh(submission);
        }
        else {
            RenderMesh(submission);
        }

        // Clear current submission
        s_CurrentRenderSubmission = nullptr;
    }
    void Renderer::RenderMesh(const DXEngine::RenderSubmission& submission)
    {
        if (!submission.mesh || !submission.mesh->IsValid())
            return;

        auto material = submission.GetEffectiveMaterial();
        if (!material)
            return;

        //Bind Material and Shader
        BindMaterial(material);
        BindShaderForMaterial(material, submission.mesh);
        //Transform buffers
        SetupTransformBuffer(submission);

        //getShader
        const void* shaderByteCode = nullptr;
        size_t byteCodeLength = 0;
        if (s_CurrentShader)
        {
            auto blob = s_CurrentShader->GetByteCode();
            if (blob)
            {
                shaderByteCode = blob->GetBufferPointer();
                byteCodeLength = blob->GetBufferSize();
            }
        }
        //bind mesh and render
        submission.mesh->Bind(shaderByteCode, byteCodeLength);
        submission.mesh->Draw(submission.submeshIndex);

        s_Stats.drawCalls++;
        s_Stats.meshesRendered++;
        s_Stats.submeshesRendered++;

        auto meshResource = submission.mesh->GetResource();
        if (meshResource && meshResource->GetIndexData())
        {
            uint32_t indexCount = submission.mesh->GetIndexCount();
            s_Stats.trianglesRendered += indexCount / 3;
        }
    }

    void Renderer::RenderInstanceMesh(const DXEngine::RenderSubmission& submission)
    {
        if (!submission.mesh || !submission.mesh->IsValid() ||
            !submission.instanceTransforms || submission.instanceCount == 0)
            return;

        auto material = submission.GetEffectiveMaterial();
        if (!material)
            return;

        //Bind material and Shader
        BindMaterial(material);
        BindShaderForMaterial(material,submission.mesh);

        //setup Transform and instance buffers
        SetupTransformBuffer(submission);
        SetupInstanceBuffer(submission);

        //Get ShaderByteCode
        const void* shaderByteCode = nullptr;
        size_t byteCodeLength = 0;
        if(s_CurrentShader)
        {
            auto blob = s_CurrentShader->GetByteCode();
            if (blob)
            {
                shaderByteCode = blob->GetBufferPointer();
                byteCodeLength = blob->GetBufferSize();
            }
        }

        //bind mesh and render instanced
        submission.mesh->Bind(shaderByteCode, byteCodeLength);
        submission.mesh->DrawInstanced(static_cast<uint32_t>(submission.instanceCount), submission.submeshIndex);

        //update statistics
        s_Stats.instanceDrawCalls++;
        s_Stats.meshesRendered++;
        s_Stats.instancesRendered += static_cast<uint32_t>(submission.submeshIndex);

        auto meshResource = submission.mesh->GetResource();
        if (meshResource && meshResource->GetIndexData())
        {
            uint32_t indexCount = submission.mesh->GetIndexCount();
            s_Stats.trianglesRendered += (indexCount / 3) * static_cast<uint32_t>(submission.instanceCount);
        }
    }

    void Renderer::RenderSkinnedMesh(const DXEngine::RenderSubmission& submission)
    {
        if (!submission.mesh || !submission.mesh->IsValid() || !submission.boneMatrices)
            return;

        auto material = submission.GetEffectiveMaterial();
        if (!material)
            return;

        // Bind material and shader
        BindMaterial(material);
        BindShaderForMaterial(material, submission.mesh);

        // Setup transform and skinning buffers
        SetupTransformBuffer(submission);
        SetupSkinnedBuffer(submission);

        // Get shader bytecode
        const void* shaderByteCode = nullptr;
        size_t byteCodeLength = 0;
        if (s_CurrentShader)
        {
            auto blob = s_CurrentShader->GetByteCode();
            if (blob)
            {
                shaderByteCode = blob->GetBufferPointer();
                byteCodeLength = blob->GetBufferSize();
            }
        }


        // Bind mesh and render
        submission.mesh->Bind(shaderByteCode, byteCodeLength);
        submission.mesh->Draw(submission.submeshIndex);

        // Update statistics
        s_Stats.drawCalls++;
        s_Stats.meshesRendered++;
        s_Stats.submeshesRendered++;

        auto meshResource = submission.mesh->GetResource();
        if (meshResource && meshResource->GetIndexData())
        {
            uint32_t indexCount = submission.mesh->GetIndexCount();
            s_Stats.trianglesRendered += indexCount / 3;
        }
    }
    
    void Renderer::RenderUIElement(const DXEngine::RenderSubmission& submission)
    {
        if (!submission.uiElement || !s_UIQuadModel)
            return;

            //save current render state
            PushRenderState();
            SetUIRenderState();

            //get UI element bounds and Properties
            const UIRect& bounds = submission.uiElement->GetBounds();

            DirectX::XMMATRIX scaleMatrix = DirectX::XMMatrixScaling(bounds.width, bounds.height, 1.0f);
            DirectX::XMMATRIX translationMatrix = DirectX::XMMatrixTranslation(bounds.x, bounds.y, 0.0f);
            DirectX::XMMATRIX modelMatrix = scaleMatrix * translationMatrix;

            // Determine material to use
            auto materialToUse = submission.GetEffectiveMaterial();
            if (!materialToUse)
                materialToUse = s_DefaultUIMaterial;

            // Handle element-specific properties
            if (auto button = std::dynamic_pointer_cast<UIButton>(submission.uiElement))
            {
                UIColor buttonColor = GetButtonColorForState(button);
                materialToUse->SetDiffuseColor({ buttonColor.r, buttonColor.g, buttonColor.b, buttonColor.a });
            }
            else if (auto panel = std::dynamic_pointer_cast<UIPanel>(submission.uiElement))
            {
                UIColor panelColor = GetPanelColor(panel);
                materialToUse->SetDiffuseColor({ panelColor.r, panelColor.g, panelColor.b, panelColor.a });
            }
            else if (auto text = std::dynamic_pointer_cast<UIText>(submission.uiElement))
            {
                UIColor textColor = text->GetColor();
                materialToUse->SetDiffuseColor({ textColor.r, textColor.g, textColor.b, textColor.a });
            }

            auto mesh = s_UIQuadModel->GetMesh();
            if (!mesh || !mesh->IsValid()) {
                OutputDebugStringA("Warning: UI quad mesh is invalid\n");
                PopRenderState();
                return;
            }

            // Bind Material and shader
            BindMaterial(materialToUse);
            BindShaderForMaterial(materialToUse,mesh);

            // Setup UI constant buffer (register b5)
            ConstantBuffer<UIConstantBuffer> uiBuffer;
            DirectX::XMMATRIX identityView = DirectX::XMMatrixIdentity();
            s_UIBufferData = std::make_unique<UIConstantBuffer>();

            s_UIBufferData->projection = DirectX::XMMatrixTranspose(modelMatrix * identityView * s_UIProjectionMatrix);
            s_UIBufferData->screenWidth = static_cast<float>(RenderCommand::GetViewportWidth());
            s_UIBufferData->screenHeight = static_cast<float>(RenderCommand::GetViewportHeight());
            s_UIBufferData->time = static_cast<float>(s_FrameCount) / 60.0f;
            s_UIBufferData->padding = 0.0f;

            uiBuffer.Initialize(s_UIBufferData.get());

            
            uiBuffer.Update(*s_UIBufferData.get());

            RenderCommand::GetContext()->VSSetConstantBuffers(BindSlot::CB_UI, 1, uiBuffer.GetAddressOf());

            // Render the UI quad
            const void* shaderByteCode = nullptr;
            size_t byteCodeLength = 0;
            if (s_CurrentShader)
            {
                auto blob = s_CurrentShader->GetByteCode();
                if (blob)
                {
                    shaderByteCode = blob->GetBufferPointer();
                    byteCodeLength = blob->GetBufferSize();
                }
            }

            mesh->EnsureGPUResources();
            mesh->Bind(shaderByteCode, byteCodeLength);
            mesh->Draw();

            PopRenderState();

            s_Stats.drawCalls++;
            s_Stats.uiElementsRendered++;
    }

    ///Bind Material and Shader managment

    void Renderer::BindMaterial(std::shared_ptr<Material> material)
    {
        if (!material)
        {
            OutputDebugStringA("Warning: Attempting to bind null material\n");
            return;
        }

        if (material != s_CurrentMaterial)
        {
            // Bind appropriate samplers for this material
            SamplerManager::Instance().BindSamplersForMaterial(material.get());
            material->Bind();


            s_CurrentMaterial = material;
            s_Stats.materialsChanged++;
        }
    }

    void Renderer::BindShaderForMaterial(std::shared_ptr<Material> material, std::shared_ptr<Mesh> mesh)
    {
        if (!s_ShaderManager || !material)
            return;

        // Get appropriate shader variant based on current mesh and material
        std::shared_ptr<ShaderProgram> shader = nullptr;

        // Get vertex layout from the mesh being rendered
        if (mesh && mesh->IsValid()) {
            const auto* vertexData = mesh->GetResource()->GetVertexData();
            if (vertexData) {
                const VertexLayout& layout = vertexData->GetLayout();
                shader = s_ShaderManager->GetShaderForMesh(layout, material.get(), material->GetType());
            }
        }

        // Fallback if mesh is invalid or shader creation failed
        if (!shader) {
            shader = s_ShaderManager->GetFallbackShader(material->GetType());
        }

        if (shader && shader != s_CurrentShader) {
            shader->Bind();
            s_CurrentShader = shader;
            s_Stats.shadersChanged++;
        }
    }

    void Renderer::SetupTransformBuffer(const DXEngine::RenderSubmission& submission)
    {
        ConstantBuffer<TransfomBufferData> vsBuffer;

        DirectX::XMMATRIX modelMatrix = DirectX::XMLoadFloat4x4(&submission.modelMatrix);

        auto camera = RenderCommand::GetCamera();
        if (!camera)
        {
            OutputDebugStringA("Warning: No camera available for transform setup\n");
            return;
        }

        auto view = camera->GetView();
        auto proj = camera->GetProjection();

        s_TransformBufferData = std::make_unique<TransfomBufferData>();
        s_TransformBufferData->WVP = DirectX::XMMatrixTranspose(modelMatrix * view * proj);
        s_TransformBufferData->Model = DirectX::XMMatrixTranspose(modelMatrix);
        s_TransformBufferData->cameraPosition = camera->GetPosition();
        s_TransformBufferData->time = s_Time;
        vsBuffer.Initialize(s_TransformBufferData.get());

        vsBuffer.Update(*s_TransformBufferData.get());

        RenderCommand::GetContext()->VSSetConstantBuffers(BindSlot::CB_Transform, 1, vsBuffer.GetAddressOf());
    }

    void Renderer::SetupInstanceBuffer(const DXEngine::RenderSubmission& submission)
    {
        if (!submission.instanceTransforms || submission.instanceCount == 0)
            return;

        auto instanceBuffer = std::make_unique<RawBuffer>();
        BufferDesc bufferDesc;
        bufferDesc.bufferType = BufferType::Vertex;
        bufferDesc.usageType = UsageType::Dynamic; 
        bufferDesc.byteWidth = static_cast<UINT>(submission.instanceCount * sizeof(DirectX::XMFLOAT4X4));
        bufferDesc.initialData = submission.instanceTransforms->data();

        if (!instanceBuffer->Initialize(bufferDesc))
        {
            return;
        }
        UINT stride = sizeof(DirectX::XMFLOAT4X4);
        UINT offset = 0;

        RenderCommand::GetContext()->IASetVertexBuffers(1, 1, instanceBuffer->GetAddressOf(), &stride, &offset);
    }

   void Renderer::SetupSkinnedBuffer(const DXEngine::RenderSubmission& submission)
   {
       if (!submission.boneMatrices || submission.boneMatrices->empty())
       {
           OutputDebugStringA("SetupSkinnedBuffer: No bone matrices available\n");
           return;
       }

       // Create a CPU-side struct matching the GPU cbuffer layout
       BoneMatrixBuffer boneData;

       // Clamp to the supported maximum
       const size_t boneCount = std::min(submission.boneMatrices->size(), size_t(128));

       // so we can copy them directly
       for (size_t i = 0; i < boneCount; ++i)
       {
           // Matrices are already in the correct format from AnimationEvaluator
           boneData.boneMatrices[i] = (*submission.boneMatrices)[i];
       }

       // Initialize remaining matrices to identity
       DirectX::XMFLOAT4X4 identity;
       DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());

       for (size_t i = boneCount; i < 128; ++i)
       {
           boneData.boneMatrices[i] = identity;
       }

       // Create and upload the constant buffer
       ConstantBuffer<BoneMatrixBuffer> boneBuffer;
       if (!boneBuffer.Initialize(&boneData))
       {
           OutputDebugStringA("SetupSkinnedBuffer: Failed to initialize bone buffer\n");
           return;
       }

       boneBuffer.Update(boneData);

       // Bind to vertex shader at slot b1 (CB_Bones)
       RenderCommand::GetContext()->VSSetConstantBuffers(
           BindSlot::CB_Bones, 1, boneBuffer.GetAddressOf());
   }

    float Renderer::CalculateDistancetoCamera(const DXEngine::RenderSubmission& submission)
    {
        auto camera = RenderCommand::GetCamera();
        if (!camera)
            return 0.0f;

        if (submission.isUIElement)
        {
            // UI elements use submission order for now
            return 0.0f;
        }

        DirectX::XMVECTOR cameraPos = camera->GetPos();
        DirectX::XMMATRIX modelMatrix = DirectX::XMLoadFloat4x4(&submission.modelMatrix);
        DirectX::XMVECTOR modelPos = DirectX::XMVector3Transform(DirectX::XMVectorZero(), modelMatrix);
        DirectX::XMVECTOR distance = DirectX::XMVector3Length(DirectX::XMVectorSubtract(modelPos, cameraPos));

        return DirectX::XMVectorGetX(distance);
    }

    uint64_t Renderer::GenarateBatchKey(const DXEngine::RenderSubmission& submission)
    {
        //create a key for batching similar objects together
        uint64_t key = 0;

        // Material type (higher priority)
        key |= (static_cast<uint64_t>(submission.material ? static_cast<int>(submission.material->GetType()) : 0) << 56);

        // Material pointer (for exact material matching)
        key |= (reinterpret_cast<uint64_t>(submission.GetEffectiveMaterial().get()) & 0x00FFFFFFFFFFFFFF);

        return key;
    }

    uint64_t Renderer::GenerateSortKey(const DXEngine::RenderSubmission& submission)
    {
        // Create a key for depth sorting
        uint32_t depthInt = static_cast<uint32_t>(submission.sortKey * 1000.0f);
        return static_cast<uint64_t>(depthInt);
    }

    ///validate submission
    void Renderer::ValidateSubmission(const DXEngine::RenderSubmission& submission)
    {
        if (!submission.IsValid())
        {
            OutputDebugStringA("Error: Invalid Render Submission\n");
            return;
        }
        if (submission.isUIElement)
        {
            if (!submission.uiElement)
            {
                OutputDebugStringA("Warning: UI submission has null UIElement\n");
            }
        }
        else
        {
            if (!submission.mesh)
            {
                OutputDebugStringA("Warning: 3D submission has null mesh\n");
                return;
            }

            if (!submission.mesh->IsValid())
            {
                OutputDebugStringA("Warning: Submission has invalid mesh\n");
                return;
            }

            if (!submission.sourceModel)
            {
                OutputDebugStringA("Warning: Submission missing source model reference\n");
            }
        }

        if (!submission.material)
        {
            OutputDebugStringA("Warning: Submission has null material\n");
            return;
        }

        if (!submission.material->IsValid())
        {
            OutputDebugStringA(("Warning: Invalid material: " + submission.material->GetName() + "\n").c_str());
        }
    }

    //render State managment
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
            RenderCommand::SetDepthTestEnabled(true);
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

    //window management
    void Renderer::OnWindowResize(int width, int height)
    {
        OutputDebugStringA(("Window resized to " + std::to_string(width) + "x" + std::to_string(height) + "\n").c_str());

        RenderCommand::Resize(width, height);
        UpdateUIProjectionMatrix(width, height);
    }

    void Renderer::UpdateUIProjectionMatrix(int width, int height)
    {
        s_UIProjectionMatrix = DirectX::XMMatrixOrthographicOffCenterLH(
            0.0f,           // Left
            (float)width,   // Right  
            (float)height,  // Bottom
            0.0f,           // Top
            -1.0f,          // Near
            1.0f            // Far
        );

        OutputDebugStringA(("UI Projection matrix updated for " + std::to_string(width) + "x" + std::to_string(height) + "\n").c_str());
    }

    void Renderer::CreateUIQuad()
    {
        try
        {
            s_UIQuadModel = Model::CreateQuad(1.0f, 1.0f);
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
            s_DefaultUIMaterial = MaterialFactory::CreateUIMaterial("DefaultUI");
            s_DefaultUIMaterial->SetRenderQueue(RenderQueue::UI);
            s_DefaultUIMaterial->SetFlag(MaterialFlags::IsTransparent, true);
            s_DefaultUIMaterial->SetFlag(MaterialFlags::IsTwoSided, true);
            s_DefaultUIMaterial->SetDiffuseColor({ 1.0f, 1.0f, 1.0f, 1.0f });
            s_DefaultUIMaterial->SetAlpha(1.0f);

            OutputDebugStringA("Default UI Material created successfully\n");
        }
        catch (const std::exception& e)
        {
            OutputDebugStringA(("Failed to create Default UI Material: " + std::string(e.what()) + "\n").c_str());
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

    void Renderer::UpdateLightCulling(const std::shared_ptr<Camera>& camera)
    {
        if (!s_LightManager || !camera)
        {
            OutputDebugStringA("UpdateLightingCulling Failed");
            return;
        }

        //create a frustum for culling 
        DirectX::BoundingFrustum frustum(camera->GetProjection());
        DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(nullptr, camera->GetView());
        DirectX::BoundingFrustum worldSpaceFrustum;
        frustum.Transform(worldSpaceFrustum, invView);
        // Perform light culling
        s_LightManager->CullLights(worldSpaceFrustum);
        s_LightManager->UpdateLightData();

        // Update stats
        s_Stats.lightsProcessed = s_LightManager->GetVisibleLightCount();

        
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
        sDX_DEBUGInfoEnabled = enable;
        OutputDebugStringA(enable ? "Debug info enabled\n" : "Debug info disabled\n");
    }

    std::string Renderer::GetDebugInfo()
    {
        std::string info = "=== Enhanced Render System Debug Info ===\n";
        info += "Frame: " + std::to_string(s_FrameCount) + "\n\n";

        // Core rendering stats
        info += "=== Rendering Statistics ===\n";
        info += "Models Submitted: " + std::to_string(s_Stats.modelsSubmitted) + "\n";
        info += "Total Submissions: " + std::to_string(s_Stats.submissionProcessed) + "\n";
        info += "Batches Processed: " + std::to_string(s_Stats.batchesProcessed) + "\n";
        info += "Draw Calls: " + std::to_string(s_Stats.drawCalls) + "\n";
        info += "Instanced Draw Calls: " + std::to_string(s_Stats.instanceDrawCalls) + "\n";
        info += "Meshes Rendered: " + std::to_string(s_Stats.meshesRendered) + "\n";
        info += "Submeshes Rendered: " + std::to_string(s_Stats.submeshesRendered) + "\n";
        info += "Instances Rendered: " + std::to_string(s_Stats.instancesRendered) + "\n";
        info += "UI Elements Rendered: " + std::to_string(s_Stats.uiElementsRendered) + "\n";
        info += "Triangles Rendered: " + std::to_string(s_Stats.trianglesRendered) + "\n\n";

        // Performance stats
        info += "=== Performance Statistics ===\n";
        info += "Material Changes: " + std::to_string(s_Stats.materialsChanged) + "\n";
        info += "Shader Changes: " + std::to_string(s_Stats.shadersChanged) + "\n";
        info += "Render State Changes: " + std::to_string(s_Stats.renderStateChanges) + "\n";

        // Calculate efficiency metrics
        if (s_Stats.drawCalls > 0)
        {
            float trianglesPerDrawCall = static_cast<float>(s_Stats.trianglesRendered) / s_Stats.drawCalls;
            info += "Avg Triangles per Draw Call: " + std::to_string(trianglesPerDrawCall) + "\n";
        }

        if (s_Stats.batchesProcessed > 0)
        {
            float submissionsPerBatch = static_cast<float>(s_Stats.submissionProcessed) / s_Stats.batchesProcessed;
            info += "Avg Submissions per Batch: " + std::to_string(submissionsPerBatch) + "\n";
        }

        info += "\n";

        // Queue breakdown
        info += "=== Render Queue Breakdown ===\n";
        for (const auto& [queue, submissions] : s_SortedQueues)
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

            info += queueName + " Queue: " + std::to_string(submissions.size()) + " submissions\n";
        }
        info += "\n";

        // Configuration info
        info += "=== Configuration ===\n";
        info += "Wireframe Mode: " + std::string(s_WireframeEnabled ? "ON" : "OFF") + "\n";
        info += "Instancing: " + std::string(s_InstanceEnabled ? "ON" : "OFF") + "\n";
        info += "Frustum Culling: " + std::string(s_FrustumCullingEnabled ? "ON" : "OFF") + "\n";
        info += "Instance Batch Size: " + std::to_string(s_InstanceBatchSize) + "\n";
        info += "\n";

        // Memory info (estimated)
        info += "=== Memory Usage (Estimated) ===\n";
        size_t submissionMemory = s_RenderSubmissions.size() * sizeof(DXEngine::RenderSubmission);
        size_t batchMemory = s_RenderBatches.size() * sizeof(RenderBatch);
        size_t totalMemory = submissionMemory + batchMemory;

        info += "Submission Storage: " + std::to_string(submissionMemory) + " bytes\n";
        info += "Batch Storage: " + std::to_string(batchMemory) + " bytes\n";
        info += "Total Renderer Memory: " + std::to_string(totalMemory) + " bytes\n";
        info += "\n";

        // Shader manager info
        if (s_ShaderManager)
        {
            info += "=== Shader System ===\n";
            info += s_ShaderManager->GetShaderInfo() + "\n";
        }

        info += "=======================================\n\n";
        return info;
    }


}