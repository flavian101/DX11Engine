#pragma once
#include "RHI/GraphicsDevice.h"
#include <d3d11.h>
#include <wrl/client.h>

namespace DXEngine::RHI
{
	using Microsoft::WRL::ComPtr;

	class D3D11Buffer : public IBuffer
	{
	public:
		D3D11Buffer(ID3D11Device* device, const BufferDesc& desc);
		~D3D11Buffer()override = default;

		//IGraphicsResource
		const std::string& GetDebugName() const override { return m_DebugName; }
		void SetDebugName(const std::string& name) override;
		uint64_t GetGPUHandle()const override {
			return reinterpret_cast<uint64_t>(m_Buffer.Get());
		}
		size_t GetMemoryUsage() const override { return m_Desc.size; }
		bool IsValid()const override { return m_Buffer != nullptr; }

		//Buffer
		BufferType GetType() const override { return m_Desc.type; }
		uint32_t GetSize()const override { return m_Desc.size; }
		uint32_t GetStride()const override { return m_Desc.stride; }

		bool Update(const void* data, uint32_t size, uint32_t offset) override;
		bool Read(void* outData, uint32_t size, uint32_t offset) override;
		void* Map() override;
		void UnMap() override;


		//D3D11-Specific
		ID3D11Buffer* GetD3D11Buffer()const { return m_Buffer.Get(); }

	private:
		ComPtr<ID3D11Buffer> m_Buffer;
		ComPtr<ID3D11Device> m_Device;
		ComPtr<ID3D11DeviceContext> m_Context;
		BufferDesc  m_Desc;
		std::string m_DebugName;

	};
}

