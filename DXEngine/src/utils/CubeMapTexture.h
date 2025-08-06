#pragma 
#include "renderer/RendererCommand.h"
#include <wrl.h>

namespace DXEngine {

	class CubeMapTexture
	{
	public:
		CubeMapTexture(const char* filename[6], UINT slot = 1u);
		~CubeMapTexture();
		void Bind();

	private:
		UINT slot;
		Microsoft::WRL::ComPtr< ID3D11ShaderResourceView> skyTextureView;
		Microsoft::WRL::ComPtr < ID3D11Texture2D>skyTexture;
	};

}