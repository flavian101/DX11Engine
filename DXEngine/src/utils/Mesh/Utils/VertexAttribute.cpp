#include "dxpch.h"
#include "VertexAttribute.h"
#include <cassert>
#include <sstream>
#include <algorithm>
#include "utils/Mesh/Resource/MeshResource.h"

namespace DXEngine
{

    //vertexLayout
    VertexLayout& VertexLayout::AddAttribute(const RHI::VertexAttribute& attribute)
    {
        assert(!m_Finalized && "cannot add attributes to finalize layout");
        m_Attributes.push_back(attribute);
        return *this;
    }

    VertexLayout& VertexLayout::AddAttribute(RHI::VertexAttributeType type, RHI::DataFormat format, uint32_t slot, bool perInstance)
    {
        return AddAttribute(RHI::VertexAttribute(type, format, "", 0, slot, perInstance));
    }

    VertexLayout& VertexLayout::Position(RHI::DataFormat format, uint32_t slot)
    {
        return AddAttribute(RHI::VertexAttributeType::Position, format, slot);
    }

    VertexLayout& VertexLayout::Normal(RHI::DataFormat format, uint32_t slot)
    {
        return AddAttribute(RHI::VertexAttributeType::Normal, format, slot);
    }

    VertexLayout& VertexLayout::Tangent(RHI::DataFormat format, uint32_t slot)
    {
        return AddAttribute(RHI::VertexAttributeType::Tangent, format, slot);
    }

    VertexLayout& VertexLayout::TexCoord(uint32_t index, RHI::DataFormat format, uint32_t slot)
    {
        RHI::VertexAttributeType type = static_cast<RHI::VertexAttributeType>(static_cast<int>(RHI::VertexAttributeType::TexCoord0) + index);
        RHI::VertexAttribute attr(type, format, "TEXCOORD", index, slot);
        return AddAttribute(attr);
    }

    VertexLayout& VertexLayout::Color(uint32_t index, RHI::DataFormat format, uint32_t slot)
    {
        RHI::VertexAttributeType type = static_cast<RHI::VertexAttributeType>(static_cast<int>(RHI::VertexAttributeType::Color0) + index);
        RHI::VertexAttribute attr(type, format, "COLOR", index, slot);
        return AddAttribute(attr);
    }

    VertexLayout& VertexLayout::BlendData(RHI::DataFormat indicesFormat, RHI::DataFormat weightsFormat, uint32_t slot)
    {
        AddAttribute(RHI::VertexAttributeType::BlendIndices, indicesFormat, slot);
        AddAttribute(RHI::VertexAttributeType::BlendWeights, weightsFormat, slot);
        return *this;
    }

    void VertexLayout::Finalize()
    {
        if (m_Finalized)
            return;

        CalculateOffsetsAndStrides();
        m_Finalized = true;
    }

    uint32_t VertexLayout::GetStride(uint32_t slot) const
    {
        auto it = m_SlotStrides.find(slot);
        return it != m_SlotStrides.end() ? it->second : 0;
    }

    bool VertexLayout::HasAttribute(RHI::VertexAttributeType type, uint32_t slot) const
    {
        return FindAttribute(type, slot) != nullptr;
    }

    const RHI::VertexAttribute* VertexLayout::FindAttribute(RHI::VertexAttributeType type, uint32_t slot) const
    {
        auto it = std::find_if(m_Attributes.begin(), m_Attributes.end(),
            [type, slot](const RHI::VertexAttribute& attr)
            {
                return attr.Type == type && attr.Slot == slot;
            });
        return it != m_Attributes.end() ? &(*it) : nullptr;

    }

    std::string VertexLayout::GetDebugString() const
    {
        std::ostringstream oss{};
        oss << "VertexLayout (" << m_Attributes.size() << " attributes):\n";

        for (size_t i = 0; i < m_Attributes.size(); ++i)
        {
            const auto& attr = m_Attributes[i];
            oss << "  [" << i << "] " << attr.SemanticName << attr.SemanticIndex
                << " (slot:" << attr.Slot << ", offset:" << attr.Offset
                << ", size:" << attr.GetSize() << ")\n";
        }

        oss << "Slot strides: ";
        for (const auto& [slot, stride] : m_SlotStrides)
        {
            oss << "slot" << slot << ":" << stride << "b ";
        }

        return oss.str();
    }

    void VertexLayout::CalculateOffsetsAndStrides()
    {
        m_SlotStrides.clear();

        std::unordered_map<uint32_t, std::vector<RHI::VertexAttribute*>> slotAttributes;
        for (auto& attr : m_Attributes)
        {
            slotAttributes[attr.Slot].push_back(&attr);
        }
        //calculate
        for (auto& [slot, attributes] : slotAttributes)
        {
            uint32_t currentOffset = 0;

            for (auto* attr : attributes)
            {
                attr->Offset = currentOffset;
                currentOffset += attr->GetSize();
            }

            m_SlotStrides[slot] = currentOffset;
        }

    }
    //basic layouts
    VertexLayout VertexLayout::CreateBasic()
    {
        VertexLayout layout;
        layout.Position()
              .Normal()
              .TexCoord(0)
              .Finalize();
        return layout;
    }

    VertexLayout VertexLayout::CreateLit()
    {
        VertexLayout layout;
        layout.Position()
              .Normal()
              .Tangent()
              .TexCoord(0)
              .Finalize();
        return layout;
    }

    VertexLayout VertexLayout::CreateUI()
    {
        VertexLayout layout;
        layout.Position(RHI::DataFormat::Float2)
              .TexCoord(0)
              .Color(0)
              .Finalize();
        return layout;
    }

    VertexLayout VertexLayout::CreateSkinned()
    {
        VertexLayout layout;
        layout.Position()
              .Normal()
              .Tangent()
              .TexCoord(0)
              .BlendData()
              .Finalize();
        return layout;
    }

    VertexLayout VertexLayout::CreateParticle()
    {
        VertexLayout layout;
        layout.Position()
              .Color(0)
              .AddAttribute(RHI::VertexAttributeType::TexCoord0, RHI::DataFormat::Float2) // Size
              .Finalize();
        return layout;
    }


    //VertexData implementation
    VertexData::VertexData(const VertexLayout& layout) : m_Layout(layout)
    {
        assert(layout.IsFinalized() && "Layout must be finalized");

        // Initialize data containers for each slot
        for (const auto& attr : layout.GetAttributes())
        {
            if (m_Data.find(attr.Slot) == m_Data.end())
            {
                m_Data[attr.Slot] = std::vector<uint8_t>();
            }
        }
    }

    void VertexData::Reserve(size_t vertexCount)
    {
        for (auto& [slot, data] : m_Data)
        {
            uint32_t stride = m_Layout.GetStride(slot);
            data.reserve(vertexCount * stride);
        }
    }

    void VertexData::Resize(size_t vertexCount)
    {
        m_VertexCount = vertexCount;

        for (auto& [slot, data] : m_Data)
        {
            uint32_t stride = m_Layout.GetStride(slot);
            data.resize(vertexCount * stride);
        }
    }

    void VertexData::Clear()
    {
        for (auto& [slot, data] : m_Data)
        {
            data.clear();
        }
        m_VertexCount = 0;
    }

    bool VertexData::IsValid() const
    {
        if (m_VertexCount == 0)
            return false;

        for (const auto& [slot, data] : m_Data)
        {
            uint32_t expectedSize = m_VertexCount * m_Layout.GetStride(slot);
            if (data.size() != expectedSize)
                return false;
        }

        return true;
    }

    /// <summary>
    /// Sets the value of a <> attribute for a specific vertex in the vertex data layout.
    /// </summary>
    /// <param name="vertexIndex">The index of the vertex whose attribute will be set.</param>
    /// <param name="type">The type of the vertex attribute to set.</param>
    /// <param name="value">The float value to assign to the attribute.</param>
    /// <param name="slot">The slot index in the vertex layout where the attribute is located.</param>
    template<>
    void VertexData::SetAttribute<float>(size_t vertexIndex, RHI::VertexAttributeType type,
        const float& value, uint32_t slot)
    {
        const RHI::VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        auto& slotData = m_Data[slot];
        uint32_t stride = m_Layout.GetStride(slot);
        uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        uint8_t* attrStart = vertexStart + attr->Offset;

        memcpy(attrStart, &value, sizeof(float));
    }

    template<>
    float VertexData::GetAttribute<float>(size_t vertexIndex, RHI::VertexAttributeType type, uint32_t slot) const
    {
        const RHI::VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        const auto& slotData = m_Data.at(slot);
        uint32_t stride = m_Layout.GetStride(slot);
        const uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        const uint8_t* attrStart = vertexStart + attr->Offset;

        float result;
        memcpy(&result, attrStart, sizeof(float));
        return result;
    }

    // Additional template specializations for integer types
    template<>
    void VertexData::SetAttribute<uint32_t>(size_t vertexIndex, RHI::VertexAttributeType type,
        const uint32_t& value, uint32_t slot)
    {
        const RHI::VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        auto& slotData = m_Data[slot];
        uint32_t stride = m_Layout.GetStride(slot);
        uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        uint8_t* attrStart = vertexStart + attr->Offset;

        memcpy(attrStart, &value, sizeof(uint32_t));
    }

   
    template<>
    void VertexData::SetAttribute<DirectX::XMFLOAT4>(size_t vertexIndex, RHI::VertexAttributeType type,
        const DirectX::XMFLOAT4& value, uint32_t slot)
    {
        const RHI::VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        auto& slotData = m_Data[slot];
        uint32_t stride = m_Layout.GetStride(slot);
        uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        uint8_t* attrStart = vertexStart + attr->Offset;

        memcpy(attrStart, &value, sizeof(DirectX::XMFLOAT4));
    }

    template<>
    void VertexData::SetAttribute<DirectX::XMFLOAT3>(size_t vertexIndex, RHI::VertexAttributeType type,
        const DirectX::XMFLOAT3& value, uint32_t slot)
    {
        const RHI::VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        auto& slotData = m_Data[slot];
        uint32_t stride = m_Layout.GetStride(slot);
        uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        uint8_t* attrStart = vertexStart + attr->Offset;

        memcpy(attrStart, &value, sizeof(DirectX::XMFLOAT3));
    }

    template<>
    void VertexData::SetAttribute<DirectX::XMFLOAT2>(size_t vertexIndex, RHI::VertexAttributeType type,
        const DirectX::XMFLOAT2& value, uint32_t slot)
    {
        const RHI::VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        auto& slotData = m_Data[slot];
        uint32_t stride = m_Layout.GetStride(slot);
        uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        uint8_t* attrStart = vertexStart + attr->Offset;

        memcpy(attrStart, &value, sizeof(DirectX::XMFLOAT2));
    }

    template<>
    void VertexData::SetAttribute<uint32_t[4]>(size_t vertexIndex, RHI::VertexAttributeType type,
        const uint32_t(&value)[4], uint32_t slot)
    {
        const RHI::VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        auto& slotData = m_Data[slot];
        uint32_t stride = m_Layout.GetStride(slot);
        uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        uint8_t* attrStart = vertexStart + attr->Offset;

        memcpy(attrStart, value, sizeof(uint32_t) * 4);
    }

    template<>
    void VertexData::SetAttribute<DirectX::XMUINT4>(size_t vertexIndex, RHI::VertexAttributeType type,
        const DirectX::XMUINT4& value, uint32_t slot)
    {
        const RHI::VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        auto& slotData = m_Data[slot];
        uint32_t stride = m_Layout.GetStride(slot);
        uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        uint8_t* attrStart = vertexStart + attr->Offset;

        memcpy(attrStart, &value, sizeof(DirectX::XMUINT4));
    }

    template<>
    void VertexData::SetAttribute<DirectX::XMINT4>(size_t vertexIndex, RHI::VertexAttributeType type,
        const DirectX::XMINT4& value, uint32_t slot)
    {
        const RHI::VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        auto& slotData = m_Data[slot];
        uint32_t stride = m_Layout.GetStride(slot);
        uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        uint8_t* attrStart = vertexStart + attr->Offset;

        memcpy(attrStart, &value, sizeof(DirectX::XMINT4));
    }

    /// <summary>
    /// Retrieves a <> attribute value from a specific vertex and slot.
    /// </summary>
    /// <param name="vertexIndex">The index of the vertex from which to retrieve the attribute.</param>
    /// <param name="type">The type of the vertex attribute to retrieve.</param>
    /// <param name="slot">The slot index where the attribute is stored.</param>
    /// <returns>The value of the specified attribute as a uint32_t.</returns>

    template<>
    uint32_t VertexData::GetAttribute<uint32_t>(size_t vertexIndex, RHI::VertexAttributeType type, uint32_t slot) const
    {
        const RHI::VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        const auto& slotData = m_Data.at(slot);
        uint32_t stride = m_Layout.GetStride(slot);
        const uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        const uint8_t* attrStart = vertexStart + attr->Offset;

        uint32_t result;
        memcpy(&result, attrStart, sizeof(uint32_t));
        return result;
    }
    template<>
    DirectX::XMFLOAT4 VertexData::GetAttribute<DirectX::XMFLOAT4>(size_t vertexIndex, RHI::VertexAttributeType type, uint32_t slot) const
    {
        const RHI::VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        const auto& slotData = m_Data.at(slot);
        uint32_t stride = m_Layout.GetStride(slot);
        const uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        const uint8_t* attrStart = vertexStart + attr->Offset;

        DirectX::XMFLOAT4 result;
        memcpy(&result, attrStart, sizeof(DirectX::XMFLOAT4));
        return result;
    }
    template<>
    DirectX::XMFLOAT3 VertexData::GetAttribute<DirectX::XMFLOAT3>(size_t vertexIndex, RHI::VertexAttributeType type, uint32_t slot) const
    {
        const RHI::VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        const auto& slotData = m_Data.at(slot);
        uint32_t stride = m_Layout.GetStride(slot);
        const uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        const uint8_t* attrStart = vertexStart + attr->Offset;

        DirectX::XMFLOAT3 result;
        memcpy(&result, attrStart, sizeof(DirectX::XMFLOAT3));
        return result;
    }

    template<>
    DirectX::XMFLOAT2 VertexData::GetAttribute<DirectX::XMFLOAT2>(size_t vertexIndex, RHI::VertexAttributeType type, uint32_t slot) const
    {
        const RHI::VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        const auto& slotData = m_Data.at(slot);
        uint32_t stride = m_Layout.GetStride(slot);
        const uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        const uint8_t* attrStart = vertexStart + attr->Offset;

        DirectX::XMFLOAT2 result;
        memcpy(&result, attrStart, sizeof(DirectX::XMFLOAT2));
        return result;
    }

    template<>
    DirectX::XMUINT4 VertexData::GetAttribute<DirectX::XMUINT4>(size_t vertexIndex, RHI::VertexAttributeType type, uint32_t slot) const
    {
        const RHI::VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        const auto& slotData = m_Data.at(slot);
        uint32_t stride = m_Layout.GetStride(slot);
        const uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        const uint8_t* attrStart = vertexStart + attr->Offset;

        DirectX::XMUINT4 result;
        memcpy(&result, attrStart, sizeof(DirectX::XMUINT4));
        return result;
    }

}