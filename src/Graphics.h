#pragma once
#include "Window.h"
#include <wrl.h>
#include <DirectXMath.h>

#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"D3DCompiler.lib")

#include <d3d11.h>
#include <d3dcompiler.h>
#include "Camera.h"
#include <memory>

class Graphics
{

public:
	Graphics(HWND hwnd,int width, int height, bool enableWireFrame);
	Graphics(const Graphics&) = delete;
	Graphics& operator=(const Graphics&) = delete;
	~Graphics();
	const Microsoft::WRL::ComPtr<ID3D11DeviceContext>& GetContext() const;
	const Microsoft::WRL::ComPtr<ID3D11Device>& GetDevice() const;

	bool Intialize();
	void Resize(int newWidth, int newHeight);
	HWND getHwnd();
	void ShowMessageBox(const wchar_t* title, const char* message);
	void PrintError(HRESULT ghr);
	void ClearDepthColor(float red, float green, float blue);
	void Draw(UINT vertexCount);
	void DrawSkybox(UINT indexCount);
	void End();
	void SetCamera(const std::shared_ptr<Camera>& camera);
	const Camera& GetCamera() const;

private:
	HWND hwnd;
	int width;
	int height;
	std::shared_ptr<Camera> m_Camera;
	Microsoft::WRL::ComPtr<ID3D11Device> pDevice;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> pContext;
	Microsoft::WRL::ComPtr<IDXGISwapChain> pSwapChain;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pRenderTarget;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> pDsv;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> wireFrame;
	Microsoft::WRL::ComPtr<ID3D11BlendState> Tranparency;
//	Microsoft::WRL::ComPtr<ID3D11RasterizerState> CCWcullMode;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> CWcullMode;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> RSCullNone;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> DSLessEqual;


	
	bool enableWireFrame = false;
};

