#pragma once
#include "renderer/RendererCommand.h"
#include <wrl.h>

namespace DXEngine {
	enum class SamplerType
	{
		Point,
		Linear,
		Anisotropic,
		Shadow,// comparison Sampler for Shadow mapping
		ShadowPCF //PCF shadow Sampler with filtering 
	};

	enum class SamplerAddressMode
	{
		Wrap = D3D11_TEXTURE_ADDRESS_WRAP,
		Mirror = D3D11_TEXTURE_ADDRESS_MIRROR,
		Clamp = D3D11_TEXTURE_ADDRESS_CLAMP,
		Border = D3D11_TEXTURE_ADDRESS_BORDER,
		MirrorOnce = D3D11_TEXTURE_ADDRESS_MIRROR_ONCE
	};

	struct SamplerDesc
	{
		SamplerType type = SamplerType::Linear;
		SamplerAddressMode addressU = SamplerAddressMode::Clamp;
		SamplerAddressMode addressV = SamplerAddressMode::Clamp;
		SamplerAddressMode addressW = SamplerAddressMode::Clamp;
		float borderColor[4] = { 1.0f,1.0f,1.0f,1.0f };
		uint32_t maxAnisotropy = 16;
		float mipLODBias = 0.0f;
		float minLOD = 0.0f;
		float maxLOD = D3D11_FLOAT32_MAX;
		D3D11_COMPARISON_FUNC comparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
	};


	class SamplerManager
	{
	public:
		static SamplerManager& Instance()
		{
			static SamplerManager instance;
			return instance;
		}

		void Initilize();
		void Shutdown();

		// Get predefined samplers
		Microsoft::WRL::ComPtr<ID3D11SamplerState> GetStandardSampler() const { return m_StandardSampler; }
		Microsoft::WRL::ComPtr<ID3D11SamplerState> GetShadowSampler() const { return m_ShadowSampler; }
		Microsoft::WRL::ComPtr<ID3D11SamplerState> GetShadowPCFSampler() const { return m_ShadowPCFSampler; }
		Microsoft::WRL::ComPtr<ID3D11SamplerState> GetPointSampler() const { return m_PointSampler; }
		Microsoft::WRL::ComPtr<ID3D11SamplerState> GetLinearSampler() const { return m_LinearSampler; }

		// Create custom sampler
		Microsoft::WRL::ComPtr<ID3D11SamplerState> CreateSampler(const SamplerDesc& desc);

		// Create custom sampler
		Microsoft::WRL::ComPtr<ID3D11SamplerState> CreateSampler(const SamplerDesc& desc);

		// Bind samplers to shader stages
		void BindPixelShaderSamplers();
		void BindVertexShaderSamplers();

	private:
		SamplerManager() = default;

		void CreateStandardSampler();
		void CreateShadowSamplers();
		void CreateFilteringSamplers();

		// Standard samplers
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_StandardSampler;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_PointSampler;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_LinearSampler;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_AnisotropicSampler;

		// Shadow samplers
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_ShadowSampler;        // Basic shadow comparison
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_ShadowPCFSampler;     // PCF filtering

	};


	class Sampler
	{
	public:
		Sampler() = default;
		Sampler(SamplerType type, SamplerAddressMode addressMode = SamplerAddressMode::Clamp);
		~Sampler();

		bool Initialized(const SamplerDesc& desc);
		void Bind(uint32_t slot = 0, bool pixelShader = true,bool vertexShader = false);
		bool IsValid() const { return m_SamplerState != nullptr;}

		ID3D11SamplerState* GetSamplerState()const { return m_SamplerState.Get();}

		void Release();

	private:
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_SamplerState;
		SamplerDesc m_Description;
	};
}
