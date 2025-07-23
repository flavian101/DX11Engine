#pragma 
#include "Graphics.h"
#include <wrl.h>

class CubeMapTexture
{
public:
	CubeMapTexture(Graphics& g, const char* filename[6], UINT slot);
	~CubeMapTexture();
	void Bind(Graphics& g);

private:
	UINT slot;
	Microsoft::WRL::ComPtr< ID3D11ShaderResourceView> skyTextureView;
	Microsoft::WRL::ComPtr < ID3D11Texture2D>skyTexture;
};

