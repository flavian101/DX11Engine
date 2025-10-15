#pragma once
#include "RHI/GraphicsDevice.h"
#include <d3d11.h>
#include <wrl/client.h>

namespace DXEngine::RHI
{
	using Microsoft::WRL::ComPtr;

	class D3D11Shader: public IShader
	{
		D3D11Shader(ID3D11Device* device, const ShaderDesc& desc);
		~D3D11Shader() override = default;

        // IGraphicsResource
        const std::string& GetDebugName() const override { return m_DebugName; }
        void SetDebugName(const std::string& name) override { m_DebugName = name; }
        uint64_t GetGPUHandle() const override {
            return reinterpret_cast<uint64_t>(m_ShaderObject.Get());
        }
        size_t GetMemoryUsage() const override {
            return m_Bytecode ? m_Bytecode->GetBufferSize() : 0;
        }
        bool IsValid() const override { return m_ShaderObject != nullptr; }

        //IShader
        ShaderStage GetStage() const override { return m_Stage; }
        const void* GetBytecode() const override {
            return m_Bytecode ? m_Bytecode->GetBufferPointer() : nullptr;
        }
        size_t GetBytecodeSize() const override {
            return m_Bytecode ? m_Bytecode->GetBufferSize() : 0;
        }
        bool IsCompiled() const override { return m_ShaderObject != nullptr; }

        // D3D11-specific
        ID3D11DeviceChild* GetD3D11Shader() const { return m_ShaderObject.Get(); }

    private:
        ComPtr<ID3D11DeviceChild> m_ShaderObject; 
        ComPtr<ID3DBlob> m_Bytecode;
        ShaderStage m_Stage;
        std::string m_DebugName;

        bool CompileFromSource(ID3D11Device* device, const ShaderDesc& desc);
	};
}
