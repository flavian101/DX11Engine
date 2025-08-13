#pragma once
#include "RendererCommand.h"
#include <memory>
#include <vector>
#include <map>
#include <utils/material/Material.h>


namespace DXEngine {


    // Forward declarations
    class ShaderManager;
    class ShaderProgram;
    class Model;
    class Mesh;
    class Camera;
    class UIElement;
    class UIButton;
    class UIText;
    class UIPanel;
    struct UIColor;

    // Improved RenderItem structure with better type safety
    struct RenderItem
    {
        // 3D Model data
        std::shared_ptr<Model> model = nullptr;
        std::shared_ptr<Material> material = nullptr;
        std::shared_ptr<Material> materialOverride = nullptr;

        // UI Element data
        std::shared_ptr<UIElement> uiElement = nullptr;

        // Common properties
        RenderQueue queue = RenderQueue::Opaque;
        float sortKey = 0.0f;
        bool isUIElement = false;
        uint32_t submeshIndex = 0;  // For multi-submesh models

        // 3D Model constructor
        RenderItem(std::shared_ptr<Model> m, std::shared_ptr<Material> mat, uint32_t submesh = 0)
            : model(m), material(mat), submeshIndex(submesh)
            , queue(mat ? mat->GetRenderQueue() : RenderQueue::Opaque)
        {
            isUIElement = false;
        }

        // UI Element constructor
        RenderItem(std::shared_ptr<UIElement> ui, std::shared_ptr<Material> mat)
            : uiElement(ui), material(mat), queue(RenderQueue::UI)
        {
            isUIElement = true;
        }

        // Validation
        bool IsValid() const
        {
            if (isUIElement)
                return uiElement != nullptr && material != nullptr;
            else
                return model != nullptr && material != nullptr;
        }
    };

    class Renderer
    {
    public:
        // Core initialization
        static void Init(HWND hwnd, int width, int height);
        static void InitShaderManager(std::shared_ptr<ShaderManager> shaderManager);
        static void Shutdown();

        // Scene management
        static void SetClearColor(float red, float green, float blue, float alpha = 1.0f);
        static void BeginScene(const std::shared_ptr<Camera>& camera);
        static void EndScene();

        // 3D Model rendering
        static void Submit(std::shared_ptr<Model> model);
        static void Submit(std::shared_ptr<Model> model, std::shared_ptr<Material> materialOverride);
        static void SubmitImmediate(std::shared_ptr<Model> model, std::shared_ptr<Material> materialOverride = nullptr);
        static void SubmitSubmesh(std::shared_ptr<Model> model, size_t submeshIndex, std::shared_ptr<Material> materialOverride = nullptr);

        // UI rendering
        static void SubmitUI(std::shared_ptr<UIElement> element);
        static void SubmitUI(std::shared_ptr<UIElement> element, std::shared_ptr<Material> materialOverride);
        static void SubmitUIImmediate(std::shared_ptr<UIElement> element, std::shared_ptr<Material> material = nullptr);

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
            uint32_t verticesRendered = 0;
            uint32_t trianglesRendered = 0;
            uint32_t materialsChanged = 0;
            uint32_t shadersChanged = 0;
            uint32_t uiElements = 0;
            uint32_t renderStateChanges = 0;
            uint32_t modelsRendered = 0;
            uint32_t submeshesRendered = 0;
        };

        static const RenderStatistics& GetStats() { return s_Stats; }
        static void ResetStats();
        static void EnableWireframe(bool enable);
        static void EnableDebugInfo(bool enable);
        static std::string GetDebugInfo();

    private:
        // Core rendering pipeline
        static void ProcessRenderQueue();
        static void SortRenderItems();
        static float CalculateSortKey(const DXEngine::RenderItem& item);
        static void ValidateRenderItem(const DXEngine::RenderItem& item);
        static void RenderItem(const DXEngine::RenderItem& item);

        // 3D rendering
        static void Render3DItem(const DXEngine::RenderItem& item);
        static void RenderModel(std::shared_ptr<Model> model, std::shared_ptr<Material> material, uint32_t submeshIndex = 0);
        static void RenderMesh(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material,
            const DirectX::XMMATRIX& modelMatrix, uint32_t submeshIndex = 0);

        // UI rendering
        static void RenderUIItem(const DXEngine::RenderItem& item);
        static void CreateUIQuad();
        static void CreateDefaultUIMaterial();
        static UIColor GetButtonColorForState(std::shared_ptr<UIButton> button);
        static UIColor GetPanelColor(std::shared_ptr<UIPanel> panel);

        // Material and shader management
        static void BindMaterial(std::shared_ptr<Material> material);
        static void BindShaderForMaterial(std::shared_ptr<Material> material);

        // Static data
        static RenderStatistics s_Stats;
        static std::shared_ptr<ShaderManager> s_ShaderManager;
        static std::vector<DXEngine::RenderItem> s_RenderItems;
        static std::map<RenderQueue, std::vector<DXEngine::RenderItem*>> s_SortedQueues;

        static std::shared_ptr<Material> s_CurrentMaterial;
        static std::shared_ptr<ShaderProgram> s_CurrentShader;
        static MaterialType s_CurrentMaterialType;

        // UI-specific resources
        static std::shared_ptr<Model> s_UIQuadModel;
        static std::shared_ptr<Material> s_DefaultUIMaterial;
        static DirectX::XMMATRIX s_UIProjectionMatrix;

        // Render state management
        struct RenderState
        {
            bool depthEnabled = true;
            bool blendEnabled = false;
            RasterizerMode rasterizerMode = RasterizerMode::SolidBackCull;
            DirectX::XMMATRIX projection = DirectX::XMMatrixIdentity();
        };
        static std::vector<RenderState> s_RenderStateStack;

        // Debug and performance
        static bool s_WireframeEnabled;
        static bool s_DebugInfoEnabled;
        static uint32_t s_FrameCount;
	};
}