#include "dxpch.h"
#include "VertexShader.h"

namespace DXEngine {

	VertexShader::VertexShader( LPCWSTR filename)
	{
		//hr = D3DCompileFromFile(
	//filename, nullptr, nullptr, "main", "vs_5_0", 0, 0, &shaderBlob, NULL);
		D3DReadFileToBlob(filename, &shaderBlob);
		if (shaderBlob == NULL)
		{
			//MessageBoxA(RenderCommand:: getHwnd(), "empty vshader blob", "ERROE", MB_OK | MB_ICONEXCLAMATION);
		}

		if (FAILED(hr))
		{
			//PrintError(hr);
		}
		RenderCommand:: GetDevice()->CreateVertexShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), NULL, &pVertexShader);
	}

	VertexShader::~VertexShader()
	{
		shaderBlob.Reset();
		pVertexShader.Reset();
	}

	void VertexShader::Bind()
	{
		RenderCommand::GetContext()->VSSetShader(pVertexShader.Get(), nullptr, 0);
	}

	ID3DBlob* VertexShader::GetByteCode()
	{
		return shaderBlob.Get();
	}

	//void VertexShader::PrintError(HRESULT vhr)
	//{
	//    LPSTR buffer;
	//    DWORD bufferSize = 0;
	//
	//    // Format the error message into the buffer
	//    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
	//        NULL, vhr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
	//        (LPSTR)&buffer, bufferSize, NULL);
	//
	//    if (buffer != NULL)
	//    {
	//        // Print the error message
	//        printf("%s\n", buffer);
	//
	//        // Free the allocated buffer
	//        LocalFree(buffer);
	//    }
	//    else
	//    {
	//        printf("Failed to format error message: %d\n", GetLastError());
	//    }
	//}
}