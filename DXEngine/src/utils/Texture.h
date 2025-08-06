#pragma once
#include "renderer/RendererCommand.h"
#include "wrl.h"

namespace DXEngine {

	class Texture
	{
	public:
		Texture( const char* filename);

		void Bind( UINT slot);
	private:
		Microsoft::WRL::ComPtr< ID3D11ShaderResourceView> textureView;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> imageTexture;
	};

}