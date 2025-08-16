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
    std::vector<RenderSubmission> Renderer::s_RenderSubmissions;
    std::map<RenderQueue, std::vector<RenderSubmission*>> Renderer::s_SortedQueues;
    std::vector<RenderBatch> Renderer::s_RenderBatches;
    std::shared_ptr<Material> Renderer::s_CurrentMaterial = nullptr;
    std::shared_ptr<ShaderProgram> Renderer::s_CurrentShader = nullptr;
    MaterialType Renderer::s_CurrentMaterialType = MaterialType::Unlit;

    std::shared_ptr<Model> Renderer::s_UIQuadModel = nullptr;
    std::shared_ptr<Material> Renderer::s_DefaultUIMaterial = nullptr;
    DirectX::XMMATRIX Renderer::s_UIProjectionMatrix = DirectX::XMMatrixIdentity();
    std::vector<Renderer::RenderState> Renderer::s_RenderStateStack;

    bool Renderer::s_WireframeEnabled = false;
    bool Renderer::s_DebugInfoEnabled = false;
    bool Renderer::s_InstanceEnabled = true;
    bool Renderer::s_FrustumCullingEnabled = true;
    size_t Renderer::s_InstanceBatchSize = 100;
    uint32_t Renderer::s_FrameCount = 0;

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

        DirectX::XMMATRIX normalMatrix = model->GetModelMatrix();
        DirectX::XMStoreFloat4x4(&submission.normalMatrix, normalMatrix);

        submission.visible = model->IsVisible();
        submission.castsShadow = model->CastsShadows();
        submission.receivesShadows = model->ReceivesShadows();
        
        return submission;
    }

    RenderSubmission RenderSubmission::CreateFromInstanceModel(const InstanceModel* model, size_t meshIndex, size_t submeshIndex)
    {
        RenderSubmission submission = CreateFromModel(model, meshIndex, submeshIndex);

        if (model && model->GetInstanceCount() > 0)
        {
            submission.instanceTransforms = &model->GetInstanceTransforms();
            submission.instanceCount = model->GetInstanceCount();
        }

        return submission;
    }

    RenderSubmission RenderSubmission::CreateFromSkinnedModel(const SkinnedModel* model, size_t meshIndex, size_t submeshIndex)
    {
        RenderSubmission submission = CreateFromModel(model, meshIndex, submeshIndex);

        if (model && !model->GetBoneMatrices().empty())
        {
            submission.boneMatrices = &model->GetBoneMatrices();
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

        CreateUIQuad();
        CreateDefaultUIMaterial();
        UpdateUIProjectionMatrix(width, height);

        s_RenderSubmissions.reserve(1000);
        s_RenderBatches.reserve(100);

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
    }

    void Renderer::Shutdown()
    {
        OutputDebugStringA("Shutting down Renderer...\n");

        s_RenderSubmissions.clear();
        s_SortedQueues.clear();
        s_RenderBatches.clear();
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
        if (!model || !model->IsValid()||!model->IsVisible())
            return;

        s_Stats.modelsSubmitted++;
        ProcessModelSubmission(model);
    }

    void Renderer::Submit(std::shared_ptr<Model> model, std::shared_ptr<Material> materialOverride)
    {
        if (!model || !model->IsValid()||model->IsVisible())
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

    void Renderer::Submit(std::shared_ptr<InstanceModel> model)
    {
        if (!model || !model->IsValid() || !model->IsVisible())
            return;

        if (model->GetInstanceCount() == 0)
        {
            return;
        }

        s_Stats.modelsSubmitted++;
        ProcessInstanceModelSubmission(model);
    }

    void Renderer::Submit(std::shared_ptr<SkinnedModel> model)
    {
        if (!model || !model->IsValid() || !model->IsVisible())
            return;


        s_Stats.modelsSubmitted++;
        ProcessSkinnedModelSubmission(model);
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
            return;


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
            return;

        model->EnsureDefaultMaterials();

        //frustum culling check
        if (s_FrustumCullingEnabled && IsModelVisible(model.get(), RenderCommand::GetCamera()))
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
                }
            }
        }

    }

    void Renderer::ProcessInstanceModelSubmission(std::shared_ptr<InstanceModel> model)
    {
        if (!model || !model->IsValid() || model->GetInstanceCount() == 0)
            return;

        model->EnsureDefaultMaterials();

        //submit all meshes as instances
        for (size_t meshIndex = 0; meshIndex < model->GetMeshCount(); ++meshIndex)
        {
            auto mesh = model->GetMesh(meshIndex);
            if (!mesh || !mesh->IsValid())
                continue;

            size_t submeshCount = std::max(size_t(1), mesh->GetSubmeshCount());
            for (size_t submeshIndex = 0; submeshIndex < submeshCount; ++submeshIndex)
            {
                DXEngine::RenderSubmission submission = DXEngine::RenderSubmission::CreateFromInstanceModel(model.get(), meshIndex, submeshIndex);
                if (submission.IsValid())
                {
                    ValidateSubmission(submission);
                    s_RenderSubmissions.push_back(submission);
                    s_Stats.submissionProcessed++;
                    s_Stats.instancesRendered+= static_cast<uint32_t>(model->GetInstanceCount());
                }
            }

        }
    }

    void Renderer::ProcessSkinnedModelSubmission(std::shared_ptr<SkinnedModel> model)
    {
        if (!model || !model->IsValid())
            return;

        model->EnsureDefaultMaterials();

        //submit all meshes as skinned
        for (size_t meshIndex = 0; meshIndex < model->GetMeshCount(); ++meshIndex)
        {
            auto mesh = model->GetMesh(meshIndex);
            if (!mesh || !mesh->IsValid())
                continue;

            size_t submeshIndexCount = std::max(size_t(1), mesh->GetSubmeshCount());
            for (size_t submeshIndex = 0; submeshIndex < submeshIndexCount; ++submeshIndex)
            {
                DXEngine::RenderSubmission submission = DXEngine::RenderSubmission::CreateFromSkinnedModel(model.get(), meshIndex, submeshIndex);

                if (submission.IsValid())
                {
                    ValidateSubmission(submission);
                    s_RenderSubmissions.push_back(submission);
                    s_Stats.submissionProcessed++;
                }

            }
        }
    }

    //culling and LOD
    bool Renderer::IsModelVisible(const Model* model, const std::shared_ptr<Camera>& camera)
    {
        if (!model || !camera)
            return true;

        //simple culling based on distance
        BoundingSphere worldSphere = model->GetWorldBoundingSphere();
        DirectX::XMVECTOR camPos = camera->GetPos();
        DirectX::XMVECTOR sphereCenter = DirectX::XMLoadFloat3(&worldSphere.center);
        DirectX::XMVECTOR distance = DirectX::XMVector3Length(DirectX::XMVectorSubtract(sphereCenter, camPos));

        float distanceValue = DirectX::XMVectorGetX(distance);
        float maxViewDistance = 500.0f;

        return distanceValue <= (maxViewDistance + worldSphere.radius);
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
        if (!submission.IsValid())
        {
            OutputDebugStringA("Warning: Attempting to render invalid submission\n");
            return;
        }
        if (submission.isUIElement)
        {
            RenderUIElement(submission);
        }
        else if (submission.instanceTransforms && submission.instanceCount > 0)
        {
            RenderInstanceMesh(submission);
        }
        else if (submission.boneMatrices)
        {
            RenderSkinnedMesh(submission);
        }
        else
        {
            RenderMesh(submission);
        }
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
        BindShaderForMaterial(material);
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
        BindShaderForMaterial(material);

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
        BindShaderForMaterial(material);

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

            //bind Material and shader
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
            uiBuffer.data.padding = 3.0f;
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
            s_Stats.uiElementsRendered++;
    }

    ///Bind Material and Shader managment

    void Renderer::BindMaterial(std::shared_ptr<Material> material)
    {
        if (!material)
        {
            OutputDebugStringA("warning: Attempting to bind null material");
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
            else if(!shader)
            {
                OutputDebugStringA(("Warning: no shader Avalilable for material type"+ 
                    std::to_string(static_cast<int>(materialType)) + "\n").c_str());

            }
        }
    }

    void Renderer::SetupTransformBuffer(const DXEngine::RenderSubmission& submission)
    {
        ConstantBuffer<TransfomBufferData> vsBuffer;
        vsBuffer.Initialize();

        DirectX::XMMATRIX modelMatrix = DirectX::XMLoadFloat4x4(&submission.modelMatrix);

        auto camera = RenderCommand::GetCamera();
        if (!camera)
        {
            OutputDebugStringA("Warning: No camera available for transform setup\n");
            return;
        }

        auto view = camera->GetView();
        auto proj = camera->GetProjection();

        vsBuffer.data.WVP = DirectX::XMMatrixTranspose(modelMatrix * view * proj);
        vsBuffer.data.Model = DirectX::XMMatrixTranspose(modelMatrix);
        vsBuffer.Update();

        RenderCommand::GetContext()->VSSetConstantBuffers(BindSlot::CB_Transform, 1, vsBuffer.GetAddressOf());
    }

    void Renderer::SetupInstanceBuffer(const DXEngine::RenderSubmission& submission)
    {
        if (!submission.instanceTransforms || submission.instanceCount == 0)
            return;

        //create instance buffer with transform matrices
        D3D11_BUFFER_DESC desc = {};
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.ByteWidth = static_cast<UINT>(submission.instanceCount * sizeof(DirectX::XMFLOAT4X4));
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = submission.instanceTransforms->data();

        Microsoft::WRL::ComPtr<ID3D11Buffer> instanceBuffer;
        HRESULT hr = RenderCommand::GetDevice()->CreateBuffer(&desc, &initData, &instanceBuffer);
        if (FAILED(hr))
        {
            OutputDebugStringA("Failed to create instance buffer\n");
            return;
        }

        UINT stride = sizeof(DirectX::XMFLOAT4X4);
        UINT offset = 0;

        RenderCommand::GetContext()->IASetVertexBuffers(1, 1, instanceBuffer.GetAddressOf(), &stride, &offset);
    }

    void Renderer::SetupSkinnedBuffer(const DXEngine::RenderSubmission& submission)
    {
        if (!submission.boneMatrices || submission.boneMatrices->empty())
            return;

        // Create bone matrix constant buffer
        ConstantBuffer<std::vector<DirectX::XMFLOAT4X4>> boneBuffer;
        boneBuffer.Initialize();
        boneBuffer.data = *submission.boneMatrices;
        boneBuffer.Update();

        RenderCommand::GetContext()->VSSetConstantBuffers(BindSlot::CB_Bones, 1, boneBuffer.GetAddressOf());
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

        auto camera = RenderCommand::GetCamera();
        if (camera)
        {
            float aspect = static_cast<float>(width) / static_cast<float>(height);
            // camera->SetAspectRatio(aspect); // Uncomment when Camera class has this method
        }
    }

    void Renderer::UpdateUIProjectionMatrix(int width, int height)
    {
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