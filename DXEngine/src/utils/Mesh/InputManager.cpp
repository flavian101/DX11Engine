#include "dxpch.h"
#include "InputManager.h"
#include "shaders/ShaderProgram.h"
#include <cassert>
#include <set>

namespace DXEngine {


    // ===== InputLayoutCache Implementation =====

    InputLayoutCache& InputLayoutCache::Instance()
    {
        static InputLayoutCache instance;
        return instance;
    }

    Microsoft::WRL::ComPtr<ID3D11InputLayout> InputLayoutCache::GetInputLayout(
        const VertexLayout& vertexLayout,
        const void* shaderByteCode,
        size_t byteCodeLength)
    {
        // Create cache key
        LayoutKey key;
        key.vertexLayoutHash = std::hash<std::string>{}(vertexLayout.GetDebugString());
        key.shaderHash = std::hash<std::string>{}(std::string((const char*)shaderByteCode, byteCodeLength));

        // Check cache
        auto it = m_Cache.find(key);
        if (it != m_Cache.end())
            return it->second;

        // Create new input layout
        auto elements = vertexLayout.CreateD3D11InputElements();

        Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
        HRESULT hr = RenderCommand::GetDevice()->CreateInputLayout(
            elements.data(),
            static_cast<UINT>(elements.size()),
            shaderByteCode,
            byteCodeLength,
            &inputLayout
        );

        if (FAILED(hr))
        {
            OutputDebugStringA("Failed to create input layout\n");
            return nullptr;
        }

        // Cache and return
        m_Cache[key] = inputLayout;
        return inputLayout;
    }

    void InputLayoutCache::ClearCache()
    {
        m_Cache.clear();
    }

}