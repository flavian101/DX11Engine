#pragma once
#include "Camera.h"
#include "dxpch.h"

namespace DXEngine {

	enum class RasterizerMode
	{
		SolidBackCull,
		SolidFrontCull,
		SolidNoCull,
		Wireframe,
	};

	class RenderCommand
	{
	public:
		static void Init(HWND hwnd, int width, int height);
		static void Shutdown();

		// Basic rendering commands
		static void SetClearColor(float red, float green, float blue, float alpha = 1.0f);
		static void Clear();
		static void Present();

		// Viewport and rendering state
		static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
		static void SetRasterizerMode(RasterizerMode mode);
		static void SetDepthLessEqual();
		static void SetDepthTestEnabled(bool enabled);
		static void SetBlendEnabled(bool enabled);
		static void SetBlendState(Microsoft::WRL::ComPtr<ID3D11BlendState> blendState);


		// Drawing commands
		static void DrawIndexed(uint32_t indexCount);

		// Window/Swapchain management
		static void Resize(int newWidth, int newHeight);

		// Device access for creating resources
		static Microsoft::WRL::ComPtr<ID3D11Device> GetDevice();
		static Microsoft::WRL::ComPtr<ID3D11DeviceContext> GetContext();

		// Camera management
		static void SetCamera(const std::shared_ptr<Camera>& camera);
		static const std::shared_ptr<Camera>& GetCamera();
		
		static int GetViewportWidth() { return s_ViewportWidth; }
		static int GetViewportHeight() { return s_ViewportHeight; }
		//D3D Wrappers
		static void CreateTexture2D();

	private:
		static bool InitializeD3D();
		static void CreateRasterizerStates();
		static void PrintError(HRESULT hr);
		static void CreateUIBlendState();

	private:
		// D3D11 Core objects
		static Microsoft::WRL::ComPtr<ID3D11Device> s_Device;
		static Microsoft::WRL::ComPtr<ID3D11DeviceContext> s_Context;
		static Microsoft::WRL::ComPtr<IDXGISwapChain> s_SwapChain;
		static Microsoft::WRL::ComPtr<ID3D11RenderTargetView> s_RenderTargetView;
		static Microsoft::WRL::ComPtr<ID3D11DepthStencilView> s_DepthStencilView;

		// States
		static Microsoft::WRL::ComPtr<ID3D11BlendState> s_TransparencyBlendState;
		static Microsoft::WRL::ComPtr<ID3D11BlendState> s_UIBlendState;
		static Microsoft::WRL::ComPtr<ID3D11DepthStencilState> s_DepthStencilState;
		static std::unordered_map<RasterizerMode, Microsoft::WRL::ComPtr<ID3D11RasterizerState>> s_RasterizerStates;

		// Window data
		static HWND s_WindowHandle;
		static int s_ViewportWidth;
		static int s_ViewportHeight;

		// Camera
		static std::shared_ptr<Camera> s_Camera;

		// Clear color
		static float s_ClearColor[4];
	};
}