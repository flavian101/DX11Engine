#pragma once
#include "RendererCommand.h"
#include <memory>
#include <vector>
#include <map>
#include <utils/material/Material.h>
#include "utils/Buffer.h"



namespace DXEngine {
    // Forward declarations
    class ShaderManager;
    class ShaderProgram;
    class Model;
    class InstanceModel;
    class SkinnedModel;
    class Mesh;
    class Camera;
    class UIElement;
    class UIButton;
    class UIText;
    class UIPanel;
    struct UIColor;
    class LightManager;

    struct RenderSubmission
    {
        // Geometry data
        std::shared_ptr<Mesh> mesh= nullptr;
        size_t meshIndex = 0;
        size_t submeshIndex = 0; 

        std::shared_ptr<Material> material = nullptr;
        std::shared_ptr<Material> materialOverride = nullptr;
        
        // === Transform Data ===
        DirectX::XMFLOAT4X4 modelMatrix;
        DirectX::XMFLOAT4X4 normalMatrix;

          //Render state
        bool visible = true;
        bool castsShadow = true;
        bool receivesShadows = true;

            //model reference
        const Model* sourceModel = nullptr;
        
            //render Queue and Sorting
        RenderQueue queue = RenderQueue::Opaque;
        float sortKey = 0.0f;
        uint64_t batchKey = 0;

            //specialized Renderring data
        const std::vector<DirectX::XMFLOAT4X4>* instanceTransforms = nullptr;
        size_t instanceCount = 0;

        const std::vector<DirectX::XMFLOAT4X4>* boneMatrices = nullptr;//skeletal animation support

             // UI Element data
        std::shared_ptr<UIElement> uiElement = nullptr;
        bool isUIElement = false;

        RenderSubmission() = default;
        static RenderSubmission CreateFromModel(const Model* model = nullptr, size_t meshIndex = 0, size_t submeshIndex = 0);
        static RenderSubmission CreateFromInstanceModel(const InstanceModel* model, size_t meshIndex, size_t subMeshIndex);
        static RenderSubmission CreateFromSkinnedModel(const SkinnedModel* model, size_t meshIndex, size_t submeshIndex);
        static RenderSubmission CreateFromUIElement(std::shared_ptr<UIElement> element, std::shared_ptr<Material> material);


        bool IsValid()const;

        std::shared_ptr<Material> GetEffectiveMaterial()const
        {
            return materialOverride ? materialOverride : material;
        }
    };

    struct RenderBatch
    {
        std::vector<RenderSubmission> submissions;
        std::shared_ptr<Material> batchMaterial = nullptr;
        std::shared_ptr<Mesh> batchMesh = nullptr;
        RenderQueue queue = RenderQueue::Opaque;
        bool requiresDepthSorting = false;
        bool IsInstanced = false;

        void Clear() { submissions.clear(); }
        bool IsEmpty() const { return submissions.empty(); }
        size_t Size()const { return submissions.size(); }
    };

    class Renderer
    {
    public:
        // Core initialization
        static void Init(HWND hwnd, int width, int height);
        static void InitLightManager();
        static std::shared_ptr<LightManager> GetLightManager() { return s_LightManager; }
        static void SetTime(float time) { s_Time = time; }
        static float GetTime() { return s_Time; }

        static void Shutdown();

        // Scene management
        static void SetClearColor(float red, float green, float blue, float alpha = 1.0f);
        static void BeginScene(const std::shared_ptr<Camera>& camera);
        static void EndScene();

        // 3D Model rendering
        static void Submit(std::shared_ptr<Model> model);
        static void Submit(std::shared_ptr<Model> model, std::shared_ptr<Material> materialOverride);
        static void SubmitMesh(std::shared_ptr<Model> model,size_t meshIndex, std::shared_ptr<Material> materialOverride = nullptr);
        static void SubmitSubmesh(std::shared_ptr<Model> model,size_t meshIndex,size_t submeshIndex, std::shared_ptr<Material> materialOverride = nullptr);
        static void Submit(std::shared_ptr<InstanceModel> model);
        static void Submit(std::shared_ptr<SkinnedModel> model);

        static void RenderImmediate(std::shared_ptr<Model> model, std::shared_ptr<Material> materialOverride = nullptr);
        static void RenderMeshImmediate(std::shared_ptr<Model> model, size_t meshIndex, std::shared_ptr<Material> materialOverride = nullptr);

        // UI rendering
        static void SubmitUI(std::shared_ptr<UIElement> element);
        static void SubmitUI(std::shared_ptr<UIElement> element, std::shared_ptr<Material> materialOverride);
        static void RenderUIImmediate(std::shared_ptr<UIElement> element, std::shared_ptr<Material> material = nullptr);

        // Render state management
        static void PushRenderState();
        static void PopRenderState();
        static void SetRenderStateForQueue(RenderQueue queue);
        static void SetUIRenderState();
        static void Set3DRenderState();

        // Window management
        static void OnWindowResize(int width, int height);
        static void UpdateUIProjectionMatrix(int width, int height);

        // Statistics and debugging
        struct RenderStatistics
        {
            uint32_t drawCalls = 0;
            uint32_t instanceDrawCalls = 0;
            uint32_t verticesRendered = 0;
            uint32_t trianglesRendered = 0;
            uint32_t materialsChanged = 0;
            uint32_t shadersChanged = 0;
            uint32_t renderStateChanges = 0;

            //model specific
            uint32_t modelsSubmitted = 0;
            uint32_t meshesRendered = 0;
            uint32_t submeshesRendered = 0;
            uint32_t instancesRendered = 0;

            //light 
            uint32_t lightsProcessed = 0;
            uint32_t lightsCulled = 0;
            uint32_t shadowMapsRendered = 0;

            //UI stats
            uint32_t uiElementsRendered = 0;

            //batch status
            uint32_t batchesProcessed = 0;
            uint32_t submissionProcessed = 0;

            //memory stats
            size_t totalMemoryUsed = 0;
        };

        static const RenderStatistics& GetStats() { return s_Stats; }
        static void ResetStats();
        static void EnableWireframe(bool enable);
        static void EnableDebugInfo(bool enable);
        static std::string GetDebugInfo();

        static void EnableInstancing(bool enable) { s_InstanceEnabled = enable; }
        static void SetInstanceBatchSize(size_t size) { s_InstanceBatchSize = size; }
        static void EnableFrustrumCulling(bool enable) { s_FrustumCullingEnabled = enable; }

    private:
        // Core rendering pipeline
        static void ProcessRenderQueue();
        static void CreateRenderBatches();
        static void SortSubmissions();
        static void ProcessRenderBatch(const DXEngine::RenderBatch& batch);

        //Submissiom processing
        static void ProcessModelSubmission(std::shared_ptr<Model> model, std::shared_ptr<Material> overrideMaterial = nullptr);
        static void ProcessInstanceModelSubmission(std::shared_ptr<InstanceModel> model);
        static void ProcessSkinnedModelSubmission(std::shared_ptr<SkinnedModel> model);

        //culling and Lod
        static bool IsModelVisible(const Model* model, const std::shared_ptr<Camera>& camera);
        static size_t SelectLODLevel(const Model* model, const std::shared_ptr<Camera>& camera);

        //Rendering methods
        static void RenderSubmission(const DXEngine::RenderSubmission& submission);
        static void RenderMesh(const DXEngine::RenderSubmission& submission);
        static void RenderInstanceMesh(const DXEngine::RenderSubmission& submission);
        static void RenderSkinnedMesh(const DXEngine::RenderSubmission& submission);
        static void RenderUIElement(const DXEngine::RenderSubmission& submission);

        //material and shader managment
        static void BindMaterial(std::shared_ptr<Material> material);
        static void BindShaderForMaterial(std::shared_ptr<Material> material);
        static void SetupTransformBuffer(const DXEngine::RenderSubmission& submission);
        static void SetupInstanceBuffer(const DXEngine::RenderSubmission& submission);
        static void SetupSkinnedBuffer(const DXEngine::RenderSubmission& submission);


        //sorting and Batching
        static float CalculateDistancetoCamera(const DXEngine::RenderSubmission& submission);
        static uint64_t GenarateBatchKey(const DXEngine::RenderSubmission& submission);
        static uint64_t GenerateSortKey(const DXEngine::RenderSubmission& submission);
        
        //validation
        static void ValidateSubmission(const DXEngine::RenderSubmission& submission);

        //UI specific
        static void CreateUIQuad();
        static void CreateDefaultUIMaterial();
        static UIColor GetButtonColorForState(std::shared_ptr<UIButton> button);
        static UIColor GetPanelColor(std::shared_ptr<UIPanel> button);

        //light culling
        static void UpdateLightCulling(const std::shared_ptr<Camera>& camera);


    private:

        static RenderStatistics s_Stats;
        static std::shared_ptr<ShaderManager> s_ShaderManager;
        static std::vector<DXEngine::RenderSubmission> s_RenderSubmissions;
        static const DXEngine::RenderSubmission* s_CurrentRenderSubmission;
        static std::map<RenderQueue, std::vector<DXEngine::RenderSubmission*>> s_SortedQueues;
        static std::vector<DXEngine::RenderBatch> s_RenderBatches;

        static std::shared_ptr<Material> s_CurrentMaterial;
        static std::shared_ptr<ShaderProgram> s_CurrentShader;
        static MaterialType s_CurrentMaterialType;
        static std::shared_ptr<TransfomBufferData> s_TransformBufferData;


        static std::shared_ptr<Model> s_UIQuadModel;
        static std::shared_ptr<Material> s_DefaultUIMaterial;
        static DirectX::XMMATRIX s_UIProjectionMatrix;
        static std::shared_ptr<UIConstantBuffer> s_UIBufferData;

        static std::shared_ptr<LightManager> s_LightManager;


        struct RenderState
        {
            bool depthEnabled = true;
            bool blendEnabled = false;
            RasterizerMode rasterizerMode = RasterizerMode::SolidBackCull;
            DirectX::XMMATRIX projection = DirectX::XMMatrixIdentity();
        };
        static std::vector<RenderState> s_RenderStateStack;
        static bool s_WireframeEnabled;
        static bool s_DebugInfoEnabled;
        static bool s_InstanceEnabled;
        static bool s_FrustumCullingEnabled;
        static size_t s_InstanceBatchSize;
        
        static uint32_t s_FrameCount;

        static float s_Time;
	};
}