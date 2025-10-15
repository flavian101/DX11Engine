#include "dxpch.h"
#include "D3D11Shader.h"


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
        DWORD flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

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
                               
        }

        return SUCCEEDED(hr);
    }

}