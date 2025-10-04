#include "dxpch.h"
#include "VertexAttribute.h"
#include <cassert>
#include <sstream>
#include <algorithm>
#include "utils/Mesh/Resource/MeshResource.h"

namespace DXEngine
{
	uint32_t VertexAttribute::GetSize() const
	{
        switch (Format)
        {
        case DataFormat::Float:     return sizeof(float);
        case DataFormat::Float2:    return sizeof(float) * 2;
        case DataFormat::Float3:    return sizeof(float) * 3;
        case DataFormat::Float4:    return sizeof(float) * 4;
        case DataFormat::Int:       return sizeof(int32_t);
        case DataFormat::Int2:      return sizeof(int32_t) * 2;
        case DataFormat::Int3:      return sizeof(int32_t) * 3;
        case DataFormat::Int4:      return sizeof(int32_t) * 4;
        case DataFormat::UByte4:    return sizeof(uint8_t) * 4;
        case DataFormat::UByte4N:   return sizeof(uint8_t) * 4;
        case DataFormat::Short2:    return sizeof(int16_t) * 2;
        case DataFormat::Short2N:   return sizeof(int16_t) * 2;
        case DataFormat::Short4:    return sizeof(int16_t) * 4;
        case DataFormat::Short4N:   return sizeof(int16_t) * 4;
        case DataFormat::Half2:     return sizeof(uint16_t) * 2; // Half precision
        case DataFormat::Half4:     return sizeof(uint16_t) * 4; // Half precision
        default:                    return 0;
        }
    }

    DXGI_FORMAT VertexAttribute::GetDXGIFormat() const
    {
        switch (Format)
        {
        case DataFormat::Float:     return DXGI_FORMAT_R32_FLOAT;
        case DataFormat::Float2:    return DXGI_FORMAT_R32G32_FLOAT;
        case DataFormat::Float3:    return DXGI_FORMAT_R32G32B32_FLOAT;
        case DataFormat::Float4:    return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case DataFormat::Int:       return DXGI_FORMAT_R32_SINT;
        case DataFormat::Int2:      return DXGI_FORMAT_R32G32_SINT;
        case DataFormat::Int3:      return DXGI_FORMAT_R32G32B32_SINT;
        case DataFormat::Int4:      return DXGI_FORMAT_R32G32B32A32_SINT;
        case DataFormat::UByte4:    return DXGI_FORMAT_R8G8B8A8_UINT;
        case DataFormat::UByte4N:   return DXGI_FORMAT_R8G8B8A8_UNORM;
        case DataFormat::Short2:    return DXGI_FORMAT_R16G16_SINT;
        case DataFormat::Short2N:   return DXGI_FORMAT_R16G16_SNORM;
        case DataFormat::Short4:    return DXGI_FORMAT_R16G16B16A16_SINT;
        case DataFormat::Short4N:   return DXGI_FORMAT_R16G16B16A16_SNORM;
        case DataFormat::Half2:     return DXGI_FORMAT_R16G16_FLOAT;
        case DataFormat::Half4:     return DXGI_FORMAT_R16G16B16A16_FLOAT;
        default:                    return DXGI_FORMAT_UNKNOWN;
        }
    }

    std::string VertexAttribute::GetDefaultSemanticName(VertexAttributeType type)
    {
        switch (type)
        {
        case VertexAttributeType::Position:     return "POSITION";
        case VertexAttributeType::Normal:       return "NORMAL";
        case VertexAttributeType::Tangent:      return "TANGENT";
        case VertexAttributeType::Bitangent:    return "BITANGENT";
        case VertexAttributeType::TexCoord0:    return "TEXCOORD";
        case VertexAttributeType::TexCoord1:    return "TEXCOORD";
        case VertexAttributeType::TexCoord2:    return "TEXCOORD";
        case VertexAttributeType::TexCoord3:    return "TEXCOORD";
        case VertexAttributeType::Color0:       return "COLOR";
        case VertexAttributeType::Color1:       return "COLOR";
        case VertexAttributeType::BlendIndices: return "BLENDINDICES";
        case VertexAttributeType::BlendWeights: return "BLENDWEIGHT";
        case VertexAttributeType::Custom:       return "CUSTOM";
        default:                                return "UNKNOWN";
        }
    }

    //vertexLayout
    VertexLayout& VertexLayout::AddAttribute(const VertexAttribute& attribute)
    {
        assert(!m_Finalized && "cannot add attributes to finalize layout");
        m_Attributes.push_back(attribute);
        return *this;
    }

    VertexLayout& VertexLayout::AddAttribute(VertexAttributeType type, DataFormat format, uint32_t slot, bool perInstance)
    {
        return AddAttribute(VertexAttribute(type, format, "", 0, slot, perInstance));
    }

    VertexLayout& VertexLayout::Position(DataFormat format, uint32_t slot)
    {
        return AddAttribute(VertexAttributeType::Position, format, slot);
    }

    VertexLayout& VertexLayout::Normal(DataFormat format, uint32_t slot)
    {
        return AddAttribute(VertexAttributeType::Normal, format, slot);
    }

    VertexLayout& VertexLayout::Tangent(DataFormat format, uint32_t slot)
    {
        return AddAttribute(VertexAttributeType::Tangent, format, slot);
    }

    VertexLayout& VertexLayout::TexCoord(uint32_t index, DataFormat format, uint32_t slot)
    {
        VertexAttributeType type = static_cast<VertexAttributeType>(static_cast<int>(VertexAttributeType::TexCoord0) + index);
        VertexAttribute attr(type, format, "TEXCOORD", index, slot);
        return AddAttribute(attr);
    }

    VertexLayout& VertexLayout::Color(uint32_t index, DataFormat format, uint32_t slot)
    {
        VertexAttributeType type = static_cast<VertexAttributeType>(static_cast<int>(VertexAttributeType::Color0) + index);
        VertexAttribute attr(type, format, "COLOR", index, slot);
        return AddAttribute(attr);
    }

    VertexLayout& VertexLayout::BlendData(DataFormat indicesFormat, DataFormat weightsFormat, uint32_t slot)
    {
        AddAttribute(VertexAttributeType::BlendIndices, indicesFormat, slot);
        AddAttribute(VertexAttributeType::BlendWeights, weightsFormat, slot);
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

    std::vector<D3D11_INPUT_ELEMENT_DESC> VertexLayout::CreateD3D11InputElements() const
    {
        assert(m_Finalized && "Layout must be finalized before creating D3D11 elements");

        std::vector<D3D11_INPUT_ELEMENT_DESC> elements;
        elements.reserve(m_Attributes.size());

        for (const auto& attr : m_Attributes)
        {
            D3D11_INPUT_ELEMENT_DESC desc = {};
            desc.SemanticName = attr.SemanticName.c_str();
            desc.SemanticIndex = attr.SemanticIndex;
            desc.Format = attr.GetDXGIFormat();
            desc.InputSlot = attr.Slot;
            desc.AlignedByteOffset = attr.Offset;
            desc.InputSlotClass = attr.PerInstance ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
            desc.InstanceDataStepRate = attr.PerInstance ? 1 : 0;

            elements.push_back(desc);
        }

        return elements;
    }

    bool VertexLayout::HasAttribute(VertexAttributeType type, uint32_t slot) const
    {
        return FindAttribute(type, slot) != nullptr;
    }

    const VertexAttribute* VertexLayout::FindAttribute(VertexAttributeType type, uint32_t slot) const
    {
        auto it = std::find_if(m_Attributes.begin(), m_Attributes.end(),
            [type, slot](const VertexAttribute& attr)
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

        std::unordered_map<uint32_t, std::vector<VertexAttribute*>> slotAttributes;
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
        layout.Position(DataFormat::Float2)
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
              .AddAttribute(VertexAttributeType::TexCoord0, DataFormat::Float2) // Size
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
    void VertexData::SetAttribute<float>(size_t vertexIndex, VertexAttributeType type,
        const float& value, uint32_t slot)
    {
        const VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        auto& slotData = m_Data[slot];
        uint32_t stride = m_Layout.GetStride(slot);
        uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        uint8_t* attrStart = vertexStart + attr->Offset;

        memcpy(attrStart, &value, sizeof(float));
    }

    template<>
    float VertexData::GetAttribute<float>(size_t vertexIndex, VertexAttributeType type, uint32_t slot) const
    {
        const VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
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
    void VertexData::SetAttribute<uint32_t>(size_t vertexIndex, VertexAttributeType type,
        const uint32_t& value, uint32_t slot)
    {
        const VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        auto& slotData = m_Data[slot];
        uint32_t stride = m_Layout.GetStride(slot);
        uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        uint8_t* attrStart = vertexStart + attr->Offset;

        memcpy(attrStart, &value, sizeof(uint32_t));
    }

   
    template<>
    void VertexData::SetAttribute<DirectX::XMFLOAT4>(size_t vertexIndex, VertexAttributeType type,
        const DirectX::XMFLOAT4& value, uint32_t slot)
    {
        const VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        auto& slotData = m_Data[slot];
        uint32_t stride = m_Layout.GetStride(slot);
        uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        uint8_t* attrStart = vertexStart + attr->Offset;

        memcpy(attrStart, &value, sizeof(DirectX::XMFLOAT4));
    }

    template<>
    void VertexData::SetAttribute<DirectX::XMFLOAT3>(size_t vertexIndex, VertexAttributeType type,
        const DirectX::XMFLOAT3& value, uint32_t slot)
    {
        const VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        auto& slotData = m_Data[slot];
        uint32_t stride = m_Layout.GetStride(slot);
        uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        uint8_t* attrStart = vertexStart + attr->Offset;

        memcpy(attrStart, &value, sizeof(DirectX::XMFLOAT3));
    }

    template<>
    void VertexData::SetAttribute<DirectX::XMFLOAT2>(size_t vertexIndex, VertexAttributeType type,
        const DirectX::XMFLOAT2& value, uint32_t slot)
    {
        const VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        auto& slotData = m_Data[slot];
        uint32_t stride = m_Layout.GetStride(slot);
        uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        uint8_t* attrStart = vertexStart + attr->Offset;

        memcpy(attrStart, &value, sizeof(DirectX::XMFLOAT2));
    }

    template<>
    void VertexData::SetAttribute<uint32_t[4]>(size_t vertexIndex, VertexAttributeType type,
        const uint32_t(&value)[4], uint32_t slot)
    {
        const VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        auto& slotData = m_Data[slot];
        uint32_t stride = m_Layout.GetStride(slot);
        uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        uint8_t* attrStart = vertexStart + attr->Offset;

        memcpy(attrStart, value, sizeof(uint32_t) * 4);
    }

    template<>
    void VertexData::SetAttribute<DirectX::XMUINT4>(size_t vertexIndex, VertexAttributeType type,
        const DirectX::XMUINT4& value, uint32_t slot)
    {
        const VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
        assert(attr && "Attribute not found in layout");
        assert(vertexIndex < m_VertexCount && "Vertex index out of range");

        auto& slotData = m_Data[slot];
        uint32_t stride = m_Layout.GetStride(slot);
        uint8_t* vertexStart = slotData.data() + (vertexIndex * stride);
        uint8_t* attrStart = vertexStart + attr->Offset;

        memcpy(attrStart, &value, sizeof(DirectX::XMUINT4));
    }

    template<>
    void VertexData::SetAttribute<DirectX::XMINT4>(size_t vertexIndex, VertexAttributeType type,
        const DirectX::XMINT4& value, uint32_t slot)
    {
        const VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
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
    uint32_t VertexData::GetAttribute<uint32_t>(size_t vertexIndex, VertexAttributeType type, uint32_t slot) const
    {
        const VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
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
    DirectX::XMFLOAT4 VertexData::GetAttribute<DirectX::XMFLOAT4>(size_t vertexIndex, VertexAttributeType type, uint32_t slot) const
    {
        const VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
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
    DirectX::XMFLOAT3 VertexData::GetAttribute<DirectX::XMFLOAT3>(size_t vertexIndex, VertexAttributeType type, uint32_t slot) const
    {
        const VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
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
    DirectX::XMFLOAT2 VertexData::GetAttribute<DirectX::XMFLOAT2>(size_t vertexIndex, VertexAttributeType type, uint32_t slot) const
    {
        const VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
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
    DirectX::XMUINT4 VertexData::GetAttribute<DirectX::XMUINT4>(size_t vertexIndex, VertexAttributeType type, uint32_t slot) const
    {
        const VertexAttribute* attr = m_Layout.FindAttribute(type, slot);
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