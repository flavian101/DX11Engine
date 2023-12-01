#pragma once
#include "Window.h"
#include <wrl.h>
#include <DirectXMath.h>

#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"D3DCompiler.lib")

#include <d3d11.h>
#include <d3dcompiler.h>

class Graphics
{

public:
	Graphics(HWND hwnd,int width, int height);
	Graphics(const Graphics&) = delete;
	Graphics& operator=(const Graphics&) = delete;
	~Graphics();
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> GetContext();
	Microsoft::WRL::ComPtr<ID3D11Device> GetDevice();

	bool Intialize();
	HWND getHwnd();
	void ShowMessageBox(const wchar_t* title, const char* message);
	void PrintError(HRESULT ghr);
	void ClearDepthColor(float red, float green, float blue);
	void Draw(UINT vertexCount);
	void End();
	void SetProjection(DirectX::FXMMATRIX proj) noexcept;
	DirectX::XMMATRIX GetProjection() const noexcept;
	void SetCamera(DirectX::FXMMATRIX view)noexcept;
	DirectX::XMMATRIX GetCamera()const noexcept;

private:
	HWND hwnd;
	int width;
	int height;
	DirectX::XMMATRIX Camera;
	DirectX::XMMATRIX projection;
	Microsoft::WRL::ComPtr<ID3D11Device> pDevice;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> pContext;
	Microsoft::WRL::ComPtr<IDXGISwapChain> pSwapChain;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pRenderTarget;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> pDsv;
};

