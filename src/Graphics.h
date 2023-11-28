#pragma once
#include "Window.h"
#include <wrl.h>

#pragma comment(lib,"d3d11.lib")

#include <d3d11.h>



class Graphics
{
public:
	Graphics(HWND hwnd);
	~Graphics();

	bool Intialize();
	void Render();

private:
	HWND hwnd;
	Microsoft::WRL::ComPtr<ID3D11Device> pDevice;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> pContext;
	Microsoft::WRL::ComPtr<IDXGISwapChain> pSwapChain;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pRenderTarget;
};

