#include "dxpch.h"
#include "InputManager.h"
#include "shaders/ShaderProgram.h"
#include <cassert>
#include <set>

namespace DXEngine {


    InputLayoutManager& InputLayoutManager::Instance()
    {
        static InputLayoutManager instance;
        return instance;
    }

    Microsoft::WRL::ComPtr<ID3D11InputLayout> InputLayoutManager::CreateInputLayout(
        const VertexLayout& vertexLayout,
        Microsoft::WRL::ComPtr<ID3DBlob> shaderByteCode)
    {
        if (!shaderByteCode)
            return nullptr;

        return CreateInputLayout(vertexLayout,
            shaderByteCode->GetBufferPointer(),
            shaderByteCode->GetBufferSize());
    }

    Microsoft::WRL::ComPtr<ID3D11InputLayout> InputLayoutManager::CreateInputLayout(
        const VertexLayout& vertexLayout,
        const void* shaderByteCode,
        size_t byteCodeSize)
    {
        if (!shaderByteCode || byteCodeSize == 0)
            return nullptr;

        return CreateInputLayoutInternal(vertexLayout, shaderByteCode, byteCodeSize);
    }

    Microsoft::WRL::ComPtr<ID3D11InputLayout> InputLayoutManager::GetInputLayout(
        const VertexLayout& vertexLayout,
        Microsoft::WRL::ComPtr<ID3DBlob> shaderByteCode)
    {
        if (!shaderByteCode)
            return nullptr;

        return GetInputLayout(vertexLayout,
            shaderByteCode->GetBufferPointer(),
            shaderByteCode->GetBufferSize());
    }

    Microsoft::WRL::ComPtr<ID3D11InputLayout> InputLayoutManager::GetInputLayout(
        const VertexLayout& vertexLayout,
        const void* shaderByteCode,
        size_t byteCodeSize)
    {
        if (!shaderByteCode || byteCodeSize == 0)
            return nullptr;

        // Create cache key
        LayoutCacheKey key;
        key.vertexLayoutHash = ComputeVertexLayoutHash(vertexLayout);
        key.shaderHash = ComputeShaderHash(shaderByteCode, byteCodeSize);

        // Check cache first
        auto it = m_LayoutCache.find(key);
        if (it != m_LayoutCache.end())
        {
            m_Stats.cacheHits++;
            return it->second;
        }

        // Cache miss - create new layout
        m_Stats.cacheMisses++;
        auto layout = CreateInputLayoutInternal(vertexLayout, shaderByteCode, byteCodeSize);

        if (layout)
        {
            m_LayoutCache[key] = layout;
            m_Stats.totalLayouts++;
        }

        return layout;
    }

    void InputLayoutManager::ClearCache()
    {
        m_LayoutCache.clear();
        m_Stats.totalLayouts = 0;
    }

    size_t InputLayoutManager::ComputeVertexLayoutHash(const VertexLayout& layout) const
    {
        // Create a hash based on the vertex layout structure
        size_t hash = 0;
        const auto& attributes = layout.GetAttributes();

        for (const auto& attr : attributes)
        {
            // Combine attribute properties into hash
            size_t attrHash = 0;
            attrHash ^= std::hash<int>{}(static_cast<int>(attr.Type)) + 0x9e3779b9 + (attrHash << 6) + (attrHash >> 2);
            attrHash ^= std::hash<int>{}(static_cast<int>(attr.Format)) + 0x9e3779b9 + (attrHash << 6) + (attrHash >> 2);
            attrHash ^= std::hash<uint32_t>{}(attr.Slot) + 0x9e3779b9 + (attrHash << 6) + (attrHash >> 2);
            attrHash ^= std::hash<uint32_t>{}(attr.Offset) + 0x9e3779b9 + (attrHash << 6) + (attrHash >> 2);
            attrHash ^= std::hash<bool>{}(attr.PerInstance) + 0x9e3779b9 + (attrHash << 6) + (attrHash >> 2);
          //  attrHash ^= std::hash<uint32_t>{}(attr.InstanceDataStepRate) + 0x9e3779b9 + (attrHash << 6) + (attrHash >> 2);

            hash ^= attrHash + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }

        return hash;
    }

    size_t InputLayoutManager::ComputeShaderHash(const void* byteCode, size_t size) const
    {
        // Simple hash of shader bytecode
        // For better performance, could hash just a portion or use a faster hash
        const char* data = static_cast<const char*>(byteCode);
        size_t hash = 0;

        // Hash in chunks for better performance on large shaders
        const size_t chunkSize = 64;
        for (size_t i = 0; i < size; i += chunkSize)
        {
            size_t end = std::min(i + chunkSize, size);
            for (size_t j = i; j < end; ++j)
            {
                hash ^= std::hash<char>{}(data[j]) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            }
        }

        return hash;
    }

    Microsoft::WRL::ComPtr<ID3D11InputLayout> InputLayoutManager::CreateInputLayoutInternal(
        const VertexLayout& vertexLayout,
        const void* shaderByteCode,
        size_t byteCodeSize)
    {
        // Convert vertex layout to D3D11 input element descriptions
        std::vector<D3D11_INPUT_ELEMENT_DESC> elements = vertexLayout.CreateD3D11InputElements();

        if (elements.empty())
        {
            OutputDebugStringA("InputLayoutManager: No input elements to create layout\n");
            return nullptr;
        }

        Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
        HRESULT hr = RenderCommand::GetDevice()->CreateInputLayout(
            elements.data(),
            static_cast<UINT>(elements.size()),
            shaderByteCode,
            byteCodeSize,
            &inputLayout
        );

        if (FAILED(hr))
        {
            OutputDebugStringA("InputLayoutManager: Failed to create input layout\n");
            return nullptr;
        }

        return inputLayout;
    }

    // ===== ScopedInputLayout Implementation =====

    ScopedInputLayout::ScopedInputLayout(const VertexLayout& layout, Microsoft::WRL::ComPtr<ID3DBlob> shaderByteCode)
    {
        m_InputLayout = InputLayoutManager::Instance().GetInputLayout(layout, shaderByteCode);

        if (m_InputLayout)
        {
            // Store previous input layout
            RenderCommand::GetContext()->IAGetInputLayout(&m_PreviousLayout);
            // Set new input layout
            RenderCommand::GetContext()->IASetInputLayout(m_InputLayout.Get());
        }
    }

    ScopedInputLayout::ScopedInputLayout(const VertexLayout& layout, const void* shaderByteCode, size_t byteCodeSize)
    {
        m_InputLayout = InputLayoutManager::Instance().GetInputLayout(layout, shaderByteCode, byteCodeSize);

        if (m_InputLayout)
        {
            // Store previous input layout
            RenderCommand::GetContext()->IAGetInputLayout(&m_PreviousLayout);
            // Set new input layout
            RenderCommand::GetContext()->IASetInputLayout(m_InputLayout.Get());
        }
    }

    ScopedInputLayout::~ScopedInputLayout()
    {
        if (m_InputLayout)
        {
            // Restore previous input layout
            RenderCommand::GetContext()->IASetInputLayout(m_PreviousLayout.Get());
        }
    }

    // ===== VertexBufferBinder Implementation =====

    VertexBufferBinder::VertexBufferBinder()
        : m_Applied(false)
    {
    }

    VertexBufferBinder::~VertexBufferBinder()
    {
        if (!m_Applied)
        {
            Apply();
        }
    }

    void VertexBufferBinder::Bind(uint32_t slot, ID3D11Buffer* buffer, uint32_t stride, uint32_t offset)
    {
        BufferBinding binding;
        binding.buffer = buffer;
        binding.stride = stride;
        binding.offset = offset;
        binding.dirty = true;

        m_Bindings[slot] = binding;
        m_Applied = false;
    }

    void VertexBufferBinder::Bind(uint32_t startSlot, const std::vector<ID3D11Buffer*>& buffers,
        const std::vector<uint32_t>& strides, const std::vector<uint32_t>& offsets)
    {
        assert(buffers.size() == strides.size());
        assert(offsets.empty() || offsets.size() == buffers.size());

        for (size_t i = 0; i < buffers.size(); ++i)
        {
            uint32_t offset = offsets.empty() ? 0 : offsets[i];
            Bind(startSlot + static_cast<uint32_t>(i), buffers[i], strides[i], offset);
        }
    }

    void VertexBufferBinder::BindFromLayout(const VertexLayout& layout,
        const std::unordered_map<uint32_t, ID3D11Buffer*>& buffers,
        const std::unordered_map<uint32_t, uint32_t>& offsets)
    {
        // Get unique slots from layout
        std::set<uint32_t> slots;
        for (const auto& attr : layout.GetAttributes())
        {
            slots.insert(attr.Slot);
        }

        // Bind buffer for each slot
        for (uint32_t slot : slots)
        {
            auto bufferIt = buffers.find(slot);
            if (bufferIt != buffers.end())
            {
                uint32_t stride = layout.GetStride(slot);
                uint32_t offset = 0;

                auto offsetIt = offsets.find(slot);
                if (offsetIt != offsets.end())
                {
                    offset = offsetIt->second;
                }

                Bind(slot, bufferIt->second, stride, offset);
            }
        }
    }

    void VertexBufferBinder::UnbindSlot(uint32_t slot)
    {
        Bind(slot, nullptr, 0, 0);
    }

    void VertexBufferBinder::UnbindSlots(uint32_t startSlot, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            UnbindSlot(startSlot + i);
        }
    }

    void VertexBufferBinder::Apply()
    {
        if (m_Applied || m_Bindings.empty())
            return;

        // Find the range of slots we need to update
        uint32_t minSlot = UINT32_MAX;
        uint32_t maxSlot = 0;

        for (const auto& [slot, binding] : m_Bindings)
        {
            if (binding.dirty)
            {
                minSlot = std::min(minSlot, slot);
                maxSlot = std::max(maxSlot, slot);
            }
        }

        if (minSlot == UINT32_MAX)
        {
            m_Applied = true;
            return;
        }

        // Create contiguous arrays for D3D11
        uint32_t rangeSize = maxSlot - minSlot + 1;
        std::vector<ID3D11Buffer*> buffers(rangeSize, nullptr);
        std::vector<UINT> strides(rangeSize, 0);
        std::vector<UINT> offsets(rangeSize, 0);

        // Fill arrays with binding data
        for (const auto& [slot, binding] : m_Bindings)
        {
            if (binding.dirty && slot >= minSlot && slot <= maxSlot)
            {
                uint32_t index = slot - minSlot;
                buffers[index] = binding.buffer;
                strides[index] = binding.stride;
                offsets[index] = binding.offset;
            }
        }

        // Apply to D3D11 context
        RenderCommand::GetContext()->IASetVertexBuffers(
            minSlot,
            rangeSize,
            buffers.data(),
            strides.data(),
            offsets.data()
        );

        // Mark all bindings as clean
        for (auto& [slot, binding] : m_Bindings)
        {
            binding.dirty = false;
        }

        m_Applied = true;
    }

}