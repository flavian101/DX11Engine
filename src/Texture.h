#pragma once

#include "Graphics.h"
#include "wrl.h"


class Texture
{
public:
	Texture(Graphics& g, const char* filename,UINT slot );

	void Bind(Graphics& g);
private:
	UINT slot;
	Microsoft::WRL::ComPtr< ID3D11ShaderResourceView> textureView;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> imageTexture;
};

