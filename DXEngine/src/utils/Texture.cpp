#include "dxpch.h"
#include "Texture.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"



namespace DXEngine {


	Texture::Texture( const char* filepath)
	{
        LoadFromFile(std::string(filepath));

	}

	Texture::Texture(const std::string& filename)
	{
		LoadFromFile(filename);
	}

	Texture::Texture(int width, int height, TextureFormat format, const void* data)
		: m_Width(width), m_Height(height), m_Format(format)
	{
		int pitch = width * GetBytesPerPixel(format);
		CreateD3D11Resources(data, pitch);
	}

	// Constructor for compressed data
	Texture::Texture(const unsigned char* data, size_t dataSize)
	{
		LoadFromMemory(data, dataSize);
	}

	void Texture::Bind(UINT slot)
	{
		RenderCommand::GetContext()->PSSetShaderResources(slot, 1, m_TextureView.GetAddressOf());
	}

    std::shared_ptr<Texture> Texture::CreateFromFile(const std::string& filepath)
    {
        auto texture = std::shared_ptr<Texture>(new Texture());
        if (texture->LoadFromFile(filepath))
        {
            return texture;
        }
        return nullptr;
    }

    std::shared_ptr<Texture> Texture::CreateFromMemory(const unsigned char* data, size_t dataSize)
    {
        auto texture = std::shared_ptr<Texture>(new Texture());
        if (texture->LoadFromMemory(data, dataSize))
        {
            return texture;
        }
        return nullptr;
    }

    std::shared_ptr<Texture> Texture::CreateFromPixels(const unsigned char* pixels, int width, int height, int channels)
    {
        auto texture = std::shared_ptr<Texture>(new Texture());
        if (texture->CreateFromPixelData(pixels, width, height, channels))
        {
            return texture;
        }
        return nullptr;
    }

    std::shared_ptr<Texture> Texture::CreateEmpty(int width, int height, TextureFormat format)
    {
        auto texture = std::shared_ptr<Texture>(new Texture());
        texture->m_Width = width;
        texture->m_Height = height;
        texture->m_Format = format;

        if (texture->CreateD3D11Resources(nullptr, 0))
        {
            return texture;
        }
        return nullptr;
    }

    // Helper method implementations
    bool Texture::LoadFromFile(const std::string& filepath)
    {
        m_FilePath = filepath;

        int channels;
        int desired_channels = 4;
        unsigned char* data = stbi_load(filepath.c_str(), &m_Width, &m_Height, &channels, desired_channels);

        if (!data)
        {
            OutputDebugStringA(("Failed to load texture: " + filepath + "\n").c_str());
            return false;
        }

        m_Format = TextureFormat::RGBA8_UNORM;
        bool success = CreateFromPixelData(data, m_Width, m_Height, desired_channels);

        stbi_image_free(data);
        return success;
    }

    bool Texture::LoadFromMemory(const unsigned char* data, size_t dataSize)
    {
        int channels;
        int desired_channels = 4;
        unsigned char* pixels = stbi_load_from_memory(data, static_cast<int>(dataSize),
            &m_Width, &m_Height, &channels, desired_channels);

        if (!pixels)
        {
            OutputDebugStringA("Failed to load texture from memory\n");
            return false;
        }

        m_Format = TextureFormat::RGBA8_UNORM;
        bool success = CreateFromPixelData(pixels, m_Width, m_Height, desired_channels);

        stbi_image_free(pixels);
        return success;
    }

    bool Texture::CreateFromPixelData(const unsigned char* pixels, int width, int height, int channels)
    {
        m_Width = width;
        m_Height = height;

        // Determine format based on channels
        switch (channels)
        {
        case 1: m_Format = TextureFormat::R8_UNORM; break;
        case 2: m_Format = TextureFormat::RG8_UNORM; break;
        case 3: m_Format = TextureFormat::RGB8_UNORM; break;
        case 4: m_Format = TextureFormat::RGBA8_UNORM; break;
        default:
            OutputDebugStringA("Unsupported number of channels\n");
            return false;
        }

        int pitch = width * channels;
        return CreateD3D11Resources(pixels, pitch);
    }

    bool Texture::CreateD3D11Resources(const void* data, int pitch)
    {
        // Create texture description
        D3D11_TEXTURE2D_DESC textureDesc = {};
        textureDesc.Width = m_Width;
        textureDesc.Height = m_Height;
        textureDesc.MipLevels = 1;
        textureDesc.ArraySize = 1;
        textureDesc.Format = GetDXGIFormat(m_Format);
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Usage = D3D11_USAGE_DEFAULT;
        textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        textureDesc.CPUAccessFlags = 0;
        textureDesc.MiscFlags = 0;

        // Create subresource data if we have pixel data
        D3D11_SUBRESOURCE_DATA* pInitialData = nullptr;
        D3D11_SUBRESOURCE_DATA initialData = {};
        if (data)
        {
            initialData.pSysMem = data;
            initialData.SysMemPitch = pitch > 0 ? pitch : m_Width * GetBytesPerPixel(m_Format);
            pInitialData = &initialData;
        }

        // Create the texture
        HRESULT hr = RenderCommand::GetDevice()->CreateTexture2D(&textureDesc, pInitialData, &m_ImageTexture);
        if (FAILED(hr))
        {
            OutputDebugStringA("Failed to create ID3D11Texture2D\n");
            return false;
        }

        // Create shader resource view
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;

        hr = RenderCommand::GetDevice()->CreateShaderResourceView(m_ImageTexture.Get(), &srvDesc, m_TextureView.GetAddressOf());
        if (FAILED(hr))
        {
            OutputDebugStringA("Failed to create ID3D11ShaderResourceView\n");
            return false;
        }

        return true;
    }

    DXGI_FORMAT Texture::GetDXGIFormat(TextureFormat format) const
    {
        switch (format)
        {
        case TextureFormat::RGBA8_UNORM:    return DXGI_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::RGB8_UNORM:     return DXGI_FORMAT_R8G8B8A8_UNORM; // D3D11 doesn't have RGB8, use RGBA8
        case TextureFormat::RG8_UNORM:      return DXGI_FORMAT_R8G8_UNORM;
        case TextureFormat::R8_UNORM:       return DXGI_FORMAT_R8_UNORM;
        case TextureFormat::RGBA16_FLOAT:   return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case TextureFormat::RGBA32_FLOAT:   return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case TextureFormat::BC1_UNORM:      return DXGI_FORMAT_BC1_UNORM;
        case TextureFormat::BC3_UNORM:      return DXGI_FORMAT_BC3_UNORM;
        case TextureFormat::BC5_UNORM:      return DXGI_FORMAT_BC5_UNORM;
        default:                            return DXGI_FORMAT_R8G8B8A8_UNORM;
        }
    }

    int Texture::GetBytesPerPixel(TextureFormat format) const
    {
        switch (format)
        {
        case TextureFormat::RGBA8_UNORM:    return 4;
        case TextureFormat::RGB8_UNORM:     return 4; // Padded to RGBA in D3D11
        case TextureFormat::RG8_UNORM:      return 2;
        case TextureFormat::R8_UNORM:       return 1;
        case TextureFormat::RGBA16_FLOAT:   return 8;
        case TextureFormat::RGBA32_FLOAT:   return 16;
        case TextureFormat::BC1_UNORM:      return 1; // Compressed format
        case TextureFormat::BC3_UNORM:      return 1; // Compressed format
        case TextureFormat::BC5_UNORM:      return 1; // Compressed format
        default:                            return 4;
        }
    }
}
