#include "Texture.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


namespace DXEngine {

	Texture::Texture(Graphics& g, const char* filename)
	{
		int image_Width, image_height, image_Channels, image_Desired_channels = 4;

		unsigned char* data = stbi_load(filename, &image_Width, &image_height,
			&image_Channels, image_Desired_channels);

		if (data == NULL)
		{
			MessageBox(g.getHwnd(), L"Failed to load the texture", L"ERROR", MB_ICONWARNING | MB_OK);
			//MessageBox(g.getHwnd(), (L"Failed to load the texture"+ (LPCWSTR)filename), L"ERROR", MB_ICONWARNING | MB_OK);
		}
		int image_pitch = image_Width * 4;

		D3D11_TEXTURE2D_DESC ts = {};
		ts.Width = image_Width;
		ts.Height = image_height;
		ts.MipLevels = 1;
		ts.ArraySize = 1;
		ts.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		ts.SampleDesc.Count = 1;
		ts.SampleDesc.Quality = 0;
		ts.Usage = D3D11_USAGE_DEFAULT;
		ts.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		ts.CPUAccessFlags = 0;
		ts.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA sts = {};
		sts.pSysMem = data;
		sts.SysMemPitch = image_pitch;

		HRESULT hr;
		hr = g.GetDevice()->CreateTexture2D(&ts, &sts, &imageTexture);

		// create the resource view on the texture
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = ts.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;

		hr = g.GetDevice()->CreateShaderResourceView(imageTexture.Get(), &srvDesc, textureView.GetAddressOf());

		stbi_image_free(data);
	}

	void Texture::Bind(Graphics& g, UINT slot)
	{
		g.GetContext()->PSSetShaderResources(slot, 1, textureView.GetAddressOf());
	}
}
