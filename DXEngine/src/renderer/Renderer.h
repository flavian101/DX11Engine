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


	struct RenderItem
	{
		std::shared_ptr<Model> model;
		std::shared_ptr<Material> material;
		std::shared_ptr<Material> materialOverride = nullptr;
		RenderQueue queue;
		float sortKey = 0.0f;

		//ui
		bool isUIElement = false;
		std::shared_ptr<UIElement> uiElement = nullptr;



		RenderItem(std::shared_ptr<Model> m, std::shared_ptr<Material> mat)
			: model(m), material(mat), queue(mat ? mat->GetRenderQueue() : RenderQueue::Opaque) {
		}

		RenderItem(std::shared_ptr<UIElement> ui, std::shared_ptr<Material> mat)
			: uiElement(ui), material(mat), queue(RenderQueue::UI), isUIElement(true) {
		}

	};


	class Renderer
	{
	public:
		static void Init(HWND hwnd, int width, int height);
		static void InitShaderManager(std::shared_ptr<ShaderManager> shaderManager);
		static void Shutdown();

		// Scene management
		static void SetClearColor(float red, float green, float blue, float alpha = 1.0f);
		static void EnableDepthTesting(bool enable);
		static void EnableBlending(bool enable);
		static void BeginScene(const std::shared_ptr<Camera>& camera);
		static void EndScene();


		//3d
		static void Submit(std::shared_ptr<Model> model);
		static void Submit(std::shared_ptr<Model> model, std::shared_ptr<Material> materialOverride);
		static void SubmitImmediate(std::shared_ptr<Model> model, std::shared_ptr<Material> material = nullptr);
		

		//UI
		static void SubmitUI(std::shared_ptr<UIElement> element);
		static void SubmitUIImmediate(std::shared_ptr<UIElement> element, std::shared_ptr<Material> material = nullptr);
		static void UpdateUIProjectionMatrix(int width, int height);


		//Render state management
		static void PushRenderState();
		static void PopRenderState();
		static void SetRenderStateForQueue(RenderQueue queue);
		static void SetUIRenderState();
		static void Set3DRenderState();

		// Window management
		static void OnWindowResize(int width, int height);


		// Statistics
		struct RenderStatistics
		{
			uint32_t drawCalls = 0;
			uint32_t verticesRendered = 0;
			uint32_t trianglesRendered = 0;
			uint32_t materialsChanged = 0;
			uint32_t shadersChanged = 0;
			uint32_t uiElements = 0;       
			uint32_t renderStateChanges = 0;
		};

		static const RenderStatistics& GetStats() {return s_Stats; }
		static void ResetStats();

		static void EnableWireframe(bool enable);
		static void EnableDebugInfo(bool enable);
		static std::string GetDebugInfo();


	private:
		static void ProcessRenderQueue();
		static void SortRenderItems();
		static float CalculateSortKey(const DXEngine::RenderItem& item);
		static void ValidateRenderItem(const DXEngine::RenderItem& item);
		static void RenderItem(const DXEngine::RenderItem& item);


		static void Render3DItem(const DXEngine::RenderItem& item);
		static void RenderUIItem(const DXEngine::RenderItem& item);


		// UI-specific helpers
		static void CreateUIQuad();
		static void CreateDefaultUIMaterial();
		static void BindUIShader();
		static bool HandleElementInputRecursive(std::shared_ptr<UIElement> element, float mouseX, float mouseY, bool leftClick, bool rightClick);
		static UIColor GetButtonColorForState(std::shared_ptr<UIButton> button);
		static UIColor GetPanelColor(std::shared_ptr<UIPanel> panel);


		static void BindMaterial(std::shared_ptr<Material> material);
		static void BindShaderForMaterial(std::shared_ptr<Material> material);


	private:
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

		// Render state stack
		struct RenderState
		{
			bool depthEnabled;
			bool blendEnabled;
			RasterizerMode rasterizerMode;
			DirectX::XMMATRIX projection;
		};

		static std::vector<RenderState> s_RenderStateStack;


		static bool s_WireframeEnabled;
		static bool s_DebugInfoEnabled;

		// Performance tracking
		static uint32_t s_FrameCount;
	};
}