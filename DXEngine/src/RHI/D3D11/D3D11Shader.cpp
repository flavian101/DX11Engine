#include "dxpch.h"
#include "D3D11Shader.h"
#include <d3d11shader.h>   // ID3D11ShaderReflection
#include <d3dcompiler.h>   // D3DReflect
#include <stdexcept>


namespace DXEngine::RHI
{
	static const char* GetShaderProfile(ShaderStage stage)
	{
		switch (stage) {
		case ShaderStage::Vertex: return "vs_5_0";
		case ShaderStage::Pixel: return "ps_5_0";
		case ShaderStage::Geometry: return "gs_5_0";
		case ShaderStage::Compute: return "cs_5_0";
		case ShaderStage::Hull: return "hs_5_0";
		case ShaderStage::Domain: return "ds_5_0";
		default: return "vs_5_0";
		}
	}

	D3D11Shader::D3D11Shader(ID3D11Device* device, const ShaderDesc& desc)
		:m_Stage(desc.stage), m_DebugName(desc.debugName)
	{
		if (!CompileFromSource(device, desc))
		{
			throw std::runtime_error("Failed to compile shader: " + desc.debugName);
		}
	}

    bool D3D11Shader::CompileFromSource(ID3D11Device* device, const ShaderDesc& desc) {
        // Setup D3D11 compilation flags
        DWORD flags = 0;

        if (desc.enableStrictness) {
            flags |= D3DCOMPILE_ENABLE_STRICTNESS;
        }

        if (desc.enableDebug) {
            flags |= D3DCOMPILE_DEBUG;
            flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
        }
        else {
            switch (desc.optimizationLevel) {
            case OptimizationLevel::None:
                flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
                break;
            case OptimizationLevel::Level1:
                flags |= D3DCOMPILE_OPTIMIZATION_LEVEL1;
                break;
            case OptimizationLevel::Level2:
                flags |= D3DCOMPILE_OPTIMIZATION_LEVEL2;
                break;
            case OptimizationLevel::Level3:
                flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
                break;
            }
        }

        ComPtr<ID3DBlob> errorBlob;
        HRESULT hr = D3DCompile(
            desc.sourceCode.c_str(),
            desc.sourceCode.length(),
            desc.debugName.c_str(),
            nullptr, // Defines
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            desc.entryPoint.c_str(),
            GetShaderProfile(desc.stage),
            flags, 0,
            m_Bytecode.GetAddressOf(),
            errorBlob.GetAddressOf()
        );

        if (FAILED(hr)) {
            if (errorBlob) {
                OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            }
            return false;
        }

        // Create shader object
        switch (desc.stage) {
        case ShaderStage::Vertex: {
            ComPtr<ID3D11VertexShader> vs;
            hr = device->CreateVertexShader(m_Bytecode->GetBufferPointer(),
                m_Bytecode->GetBufferSize(), nullptr, &vs);
            m_ShaderObject = vs;
            break;
        }
        case ShaderStage::Pixel: {
            ComPtr<ID3D11PixelShader> ps;
            hr = device->CreatePixelShader(m_Bytecode->GetBufferPointer(),
                m_Bytecode->GetBufferSize(), nullptr, &ps);
            m_ShaderObject = ps;
            break;
        }
        default:
            // Geometry / hull / domain / compute shaders are not created by this backend yet. This used to
            // fall through and report SUCCESS with a null shader object.
            OutputDebugStringA(("D3D11Shader: stage not supported by the D3D11 backend yet: " + desc.debugName + "\n").c_str());
            return false;
        }

        if (FAILED(hr))
            return false;

        BuildReflection(desc);
        return true;
    }

    void D3D11Shader::BuildReflection(const ShaderDesc& desc)
    {
        m_Reflection = ReflectionData{};
        m_Reflection.entryPoint = desc.entryPoint;

        ComPtr<ID3D11ShaderReflection> reflector;
        if (FAILED(D3DReflect(m_Bytecode->GetBufferPointer(), m_Bytecode->GetBufferSize(),
                              IID_ID3D11ShaderReflection, reinterpret_cast<void**>(reflector.GetAddressOf()))))
        {
            OutputDebugStringA(("D3D11Shader: reflection unavailable for " + desc.debugName + "\n").c_str());
            return;   // not fatal: the shader itself works, the reflection data is just empty
        }

        D3D11_SHADER_DESC shaderDesc = {};
        if (FAILED(reflector->GetDesc(&shaderDesc)))
            return;

        // bound resources -> bindings
        for (UINT i = 0; i < shaderDesc.BoundResources; ++i)
        {
            D3D11_SHADER_INPUT_BIND_DESC bind = {};
            if (FAILED(reflector->GetResourceBindingDesc(i, &bind)))
                continue;

            DescriptorBinding binding;
            binding.stage = m_Stage;
            binding.slot  = bind.BindPoint;
            binding.count = bind.BindCount;
            binding.name  = bind.Name ? bind.Name : "";

            switch (bind.Type)
            {
            case D3D_SIT_CBUFFER:                 binding.type = DescriptorType::ConstantBuffer; break;
            case D3D_SIT_TEXTURE:                 binding.type = DescriptorType::Texture;        break;
            case D3D_SIT_SAMPLER:                 binding.type = DescriptorType::Sampler;        break;
            case D3D_SIT_UAV_RWTYPED:             binding.type = DescriptorType::StorageTexture; break;
            case D3D_SIT_TBUFFER:
            case D3D_SIT_STRUCTURED:
            case D3D_SIT_BYTEADDRESS:
            case D3D_SIT_UAV_RWSTRUCTURED:
            case D3D_SIT_UAV_RWBYTEADDRESS:
            case D3D_SIT_UAV_APPEND_STRUCTURED:
            case D3D_SIT_UAV_CONSUME_STRUCTURED:
            case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
                                                  binding.type = DescriptorType::StorageBuffer;  break;
            default:                              continue;   // resource kinds the RHI has no descriptor type for
            }
            m_Reflection.bindings.push_back(std::move(binding));
        }

        // constantBufferSize = total bytes of all constant buffers the shader declares
        for (UINT i = 0; i < shaderDesc.ConstantBuffers; ++i)
        {
            ID3D11ShaderReflectionConstantBuffer* cb = reflector->GetConstantBufferByIndex(i);
            D3D11_SHADER_BUFFER_DESC cbDesc = {};
            if (cb && SUCCEEDED(cb->GetDesc(&cbDesc)))
                m_Reflection.constantBufferSize += cbDesc.Size;
        }
    }

}