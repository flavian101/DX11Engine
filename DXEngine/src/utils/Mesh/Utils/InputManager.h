#pragma once
#include "VertexAttribute.h"
#include "renderer/RendererCommand.h"
#include <memory>
#include <unordered_map>
#include <functional>

namespace DXEngine {

    // Input layout cache for shader compatibility
    class InputLayoutCache
    {
    public:
        static InputLayoutCache& Instance();

        // Get or create input layout for vertex layout + shader combination
        Microsoft::WRL::ComPtr<ID3D11InputLayout> GetInputLayout(
            const VertexLayout& vertexLayout,
            const void* shaderByteCode,
            size_t byteCodeLength);

        void ClearCache();

    private:
        InputLayoutCache() = default;

        struct LayoutKey
        {
            size_t vertexLayoutHash;
            size_t shaderHash;

            bool operator==(const LayoutKey& other) const
            {
                return vertexLayoutHash == other.vertexLayoutHash && shaderHash == other.shaderHash;
            }
        };

        struct LayoutKeyHasher
        {
            size_t operator()(const LayoutKey& key) const
            {
                return key.vertexLayoutHash ^ (key.shaderHash << 1);
            }
        };

        std::unordered_map<LayoutKey, Microsoft::WRL::ComPtr<ID3D11InputLayout>, LayoutKeyHasher> m_Cache;
    };


}