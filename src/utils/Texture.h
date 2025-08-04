#pragma once

#include "Graphics.h"
#include "wrl.h"

namespace DXEngine {

	class Texture
	{
	public:
		Texture(Graphics& g, const char* filename);

		void Bind(Graphics& g, UINT slot);
	private:
		Microsoft::WRL::ComPtr< ID3D11ShaderResourceView> textureView;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> imageTexture;
	};

}