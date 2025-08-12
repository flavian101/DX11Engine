#pragma once
#include "VertexAttribute.h"
#include "renderer/RendererCommand.h"
#include <memory>
#include <unordered_map>
#include <functional>

namespace DXEngine {

    // Forward declarations
    class ShaderProgram;

    // Input layout creation and management
    class InputLayoutManager
    {
    public:
        static InputLayoutManager& Instance();

        // Create input layout for a vertex layout and shader combination
        Microsoft::WRL::ComPtr<ID3D11InputLayout> CreateInputLayout(
            const VertexLayout& vertexLayout,
            Microsoft::WRL::ComPtr<ID3DBlob> shaderByteCode);

        Microsoft::WRL::ComPtr<ID3D11InputLayout> CreateInputLayout(
            const VertexLayout& vertexLayout,
            const void* shaderByteCode,
            size_t byteCodeSize);

        // Get cached input layout (creates if doesn't exist)
        Microsoft::WRL::ComPtr<ID3D11InputLayout> GetInputLayout(
            const VertexLayout& vertexLayout,
            Microsoft::WRL::ComPtr<ID3DBlob> shaderByteCode);

        Microsoft::WRL::ComPtr<ID3D11InputLayout> GetInputLayout(
            const VertexLayout& vertexLayout,
            const void* shaderByteCode,
            size_t byteCodeSize);

        // Cache management
        void ClearCache();
        size_t GetCacheSize() const { return m_LayoutCache.size(); }

        // Statistics
        struct Statistics
        {
            size_t cacheHits = 0;
            size_t cacheMisses = 0;
            size_t totalLayouts = 0;

            float GetHitRatio() const
            {
                return (cacheHits + cacheMisses) > 0 ?
                    static_cast<float>(cacheHits) / (cacheHits + cacheMisses) : 0.0f;
            }
        };

        const Statistics& GetStatistics() const { return m_Stats; }
        void ResetStatistics() { m_Stats = {}; }

    private:
        InputLayoutManager() = default;
        ~InputLayoutManager() = default;

        // Cache key for input layouts
        struct LayoutCacheKey
        {
            size_t vertexLayoutHash;
            size_t shaderHash;

            bool operator==(const LayoutCacheKey& other) const
            {
                return vertexLayoutHash == other.vertexLayoutHash &&
                    shaderHash == other.shaderHash;
            }
        };

        struct LayoutCacheKeyHasher
        {
            size_t operator()(const LayoutCacheKey& key) const
            {
                return key.vertexLayoutHash ^ (key.shaderHash << 1);
            }
        };

        // Helper methods
        size_t ComputeVertexLayoutHash(const VertexLayout& layout) const;
        size_t ComputeShaderHash(const void* byteCode, size_t size) const;

        Microsoft::WRL::ComPtr<ID3D11InputLayout> CreateInputLayoutInternal(
            const VertexLayout& vertexLayout,
            const void* shaderByteCode,
            size_t byteCodeSize);

    private:
        std::unordered_map<LayoutCacheKey, Microsoft::WRL::ComPtr<ID3D11InputLayout>, LayoutCacheKeyHasher> m_LayoutCache;
        mutable Statistics m_Stats;
    };

    // RAII wrapper for input layout binding
    class ScopedInputLayout
    {
    public:
        ScopedInputLayout(const VertexLayout& layout, Microsoft::WRL::ComPtr<ID3DBlob> shaderByteCode);
        ScopedInputLayout(const VertexLayout& layout, const void* shaderByteCode, size_t byteCodeSize);
        ~ScopedInputLayout();

        // Non-copyable but movable
        ScopedInputLayout(const ScopedInputLayout&) = delete;
        ScopedInputLayout& operator=(const ScopedInputLayout&) = delete;
        ScopedInputLayout(ScopedInputLayout&&) = default;
        ScopedInputLayout& operator=(ScopedInputLayout&&) = default;

        bool IsValid() const { return m_InputLayout != nullptr; }
        ID3D11InputLayout* Get() const { return m_InputLayout.Get(); }

    private:
        Microsoft::WRL::ComPtr<ID3D11InputLayout> m_InputLayout;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> m_PreviousLayout;
    };

    // Utility class for managing vertex buffer bindings
    class VertexBufferBinder
    {
    public:
        VertexBufferBinder();
        ~VertexBufferBinder();

        // Bind single vertex buffer
        void Bind(uint32_t slot, ID3D11Buffer* buffer, uint32_t stride, uint32_t offset = 0);

        // Bind multiple vertex buffers
        void Bind(uint32_t startSlot, const std::vector<ID3D11Buffer*>& buffers,
            const std::vector<uint32_t>& strides, const std::vector<uint32_t>& offsets);

        // Convenience method for mesh buffers
        void BindFromLayout(const VertexLayout& layout,
            const std::unordered_map<uint32_t, ID3D11Buffer*>& buffers,
            const std::unordered_map<uint32_t, uint32_t>& offsets = {});

        // Clear specific slots
        void UnbindSlot(uint32_t slot);
        void UnbindSlots(uint32_t startSlot, uint32_t count);

        // Apply all bindings (called automatically in destructor)
        void Apply();

    private:
        struct BufferBinding
        {
            ID3D11Buffer* buffer = nullptr;
            uint32_t stride = 0;
            uint32_t offset = 0;
            bool dirty = false;
        };

        std::unordered_map<uint32_t, BufferBinding> m_Bindings;
        bool m_Applied = false;
    };

}