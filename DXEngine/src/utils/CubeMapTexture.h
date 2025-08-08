#pragma 
#include "renderer/RendererCommand.h"
#include <wrl.h>

namespace DXEngine {

	class CubeMapTexture
	{
	public:
		CubeMapTexture(const char* filename[6]);
		~CubeMapTexture();
		void Bind(UINT slot);

	private:
		Microsoft::WRL::ComPtr< ID3D11ShaderResourceView> skyTextureView;
		Microsoft::WRL::ComPtr < ID3D11Texture2D>skyTexture;
	};

}