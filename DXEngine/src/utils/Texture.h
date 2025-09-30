#pragma once
#include "renderer/RendererCommand.h"
#include "wrl.h"
#include "material/materialTypes.h"

namespace DXEngine {
	enum class TextureFormat
	{
		RGBA8_UNORM,
		RGB8_UNORM,
		RG8_UNORM,
		R8_UNORM,
		RGBA16_FLOAT,
		RGBA32_FLOAT,
		BC1_UNORM,      // DXT1
		BC3_UNORM,      // DXT5
		BC5_UNORM       // Normal maps
	};

	enum class TextureType
	{
		Unknown,
		Diffuse,
		Normal,
		Specular,
		Roughness,
		Metallic,
		AmbientOcclusion,
		Height,
		Emissive,
		Opacity,
		DetailMask,
		Subsurface,
		Anisotropy,
		Clearcoat,
		Environment
	};

	class Texture
	{
	public:
		Texture( const char* filepath);

		Texture(const std::string& filename);
		Texture(int width, int height, TextureFormat format, const void* data = nullptr);
		Texture(const unsigned char* data, size_t dataSize); // For compressed data


		void Bind(UINT slot, bool vertexShader = false, bool pixelShader = true);

		static std::shared_ptr<Texture> CreateFromFile(const std::string& filepath);
		static std::shared_ptr<Texture> CreateFromMemory(const unsigned char* data, size_t dataSize);
		static std::shared_ptr<Texture> CreateFromPixels(const unsigned char* pixels, int width, int height, int channels = 4);
		static std::shared_ptr<Texture> CreateEmpty(int width, int height, TextureFormat format = TextureFormat::RGBA8_UNORM);

		// Texture information
		int GetWidth() const { return m_Width; }
		int GetHeight() const { return m_Height; }
		TextureFormat GetFormat() const { return m_Format; }
		const std::string& GetFilePath() const { return m_FilePath; }
		bool IsValid() const { return m_TextureView != nullptr; }

		// DirectX resources access
		ID3D11ShaderResourceView* GetShaderResourceView() const { return m_TextureView.Get(); }
		ID3D11Texture2D* GetTexture2D() const { return m_ImageTexture.Get(); }

	private:
		// Private constructor for internal use
		Texture() = default;

		// Helper methods
		bool LoadFromFile(const std::string& filepath);
		bool LoadFromMemory(const unsigned char* data, size_t dataSize);
		bool CreateFromPixelData(const unsigned char* pixels, int width, int height, int channels);
		bool CreateD3D11Resources(const void* data, int pitch = 0);
		DXGI_FORMAT GetDXGIFormat(TextureFormat format) const;
		int GetBytesPerPixel(TextureFormat format) const;

	private:
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_TextureView;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> m_ImageTexture;

		int m_Width = 0;
		int m_Height = 0;
		TextureFormat m_Format = TextureFormat::RGBA8_UNORM;
		std::string m_FilePath;
	};

	namespace TextureUtils
	{
		 TextureType DetectTextureType(const std::string& filename);
		 std::string GetTextureTypeName(TextureType type);
		 TextureSlot GetTextureSlot(TextureType type);
		 bool IsValidTextureFormat(const std::string& extension);
	};
	
	

}