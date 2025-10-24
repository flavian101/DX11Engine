#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace DXEngine::RHI
{
    // ============================================================================
    // Core Enums (your existing ones, kept as-is)
    // ============================================================================

    enum class GraphicsAPI
    {
        DirectX11,
        DirectX12,
        Vulkan,  // Fixed typo
        OpenGL,
        Metal    // For future macOS support
    };

    enum class BufferType
    {
        Vertex,
        Index,
        Uniform,
        Storage,
        Staging,
        Indirect     //: For indirect draw commands
    };

    enum class BufferUsage
    {
        Static,
        Dynamic,
        Stream
    };

    // ============================================================================
    //: Sampler States
    // ============================================================================

    enum class FilterMode
    {
        Point,
        Linear,
        Anisotropic
    };

    enum class AddressMode
    {
        Wrap,
        Mirror,
        Clamp,
        Border
    };

    enum class ComparisonFunc
    {
        Never,
        Less,
        Equal,
        LessEqual,
        Greater,
        NotEqual,
        GreaterEqual,
        Always
    };

    struct SamplerDesc
    {
        FilterMode minFilter = FilterMode::Linear;
        FilterMode magFilter = FilterMode::Linear;
        FilterMode mipFilter = FilterMode::Linear;

        AddressMode addressU = AddressMode::Wrap;
        AddressMode addressV = AddressMode::Wrap;
        AddressMode addressW = AddressMode::Wrap;

        float mipLODBias = 0.0f;
        uint32_t maxAnisotropy = 16;

        ComparisonFunc comparisonFunc = ComparisonFunc::Never;
        bool enableComparison = false;

        float borderColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        float minLOD = 0.0f;
        float maxLOD = 1000.0f;

        std::string debugName;
    };

    //Stencil
    enum class StencilOp {
        Keep,
        Zero,
        Replace,
        IncrSat,    // clamp at max
        DecrSat,    // clamp at 0
        Invert,
        IncrWrap,   // wrap on overflow
        DecrWrap    // wrap on underflow
    };

    // ============================================================================
    //: Render Pass System (for better multi-API support)
    // ============================================================================

    enum class LoadOp
    {
        Load,      // Preserve existing contents
        Clear,     // Clear to specified value
        DontCare   // Don't care about previous contents
    };

    enum class StoreOp
    {
        Store,     // Store render results
        DontCare   // Don't need to keep results
    };

    struct AttachmentDesc
    {
        TextureFormat format;
        LoadOp loadOp = LoadOp::Clear;
        StoreOp storeOp = StoreOp::Store;
        LoadOp stencilLoadOp = LoadOp::DontCare;
        StoreOp stencilStoreOp = StoreOp::DontCare;

        // Clear values
        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        float clearDepth = 1.0f;
        uint8_t clearStencil = 0;
    };

    struct RenderPassDesc
    {
        std::vector<AttachmentDesc> colorAttachments;
        AttachmentDesc depthStencilAttachment;
        bool hasDepthStencil = false;

        std::string debugName;
    };

    // ============================================================================
    //: Descriptor/Binding System
    // ============================================================================

    enum class DescriptorType
    {
        ConstantBuffer,
        Texture,
        Sampler,
        StorageBuffer,
        StorageTexture
    };

    struct DescriptorBinding
    {
        DescriptorType type;
        ShaderStage stage;
        uint32_t slot;
        uint32_t count = 1;  // For arrays

        std::string name;  // For reflection/debugging
    };

    struct DescriptorSetLayout
    {
        std::vector<DescriptorBinding> bindings;
        std::string debugName;
    };

    // ============================================================================
    // Enhanced Pipeline Description
    // ============================================================================

    struct PipelineDesc
    {
        // Shader Stages
        class IShader* vertexShader = nullptr;
        class IShader* pixelShader = nullptr;
        class IShader* geometryShader = nullptr;
        class IShader* hullShader = nullptr;      //
        class IShader* domainShader = nullptr;    //
        class IShader* computeShader = nullptr;

        // Input Layout
        std::vector<VertexAttribute> inputLayout;

        // Rasterizer State
        RasterizerState rasterizer;

        // Depth-stencil state
        DepthTestMode depthTest = DepthTestMode::Less;
        bool depthWrite = true;

        //: Stencil state
        bool stencilEnable = false;
        uint8_t stencilReadMask = 0xFF;
        uint8_t stencilWriteMask = 0xFF;
        struct StencilOpDesc
        {
            StencilOp failOp = StencilOp::Keep;
            StencilOp depthFailOp = StencilOp::Keep;
            StencilOp passOp = StencilOp::Keep;
            ComparisonFunc func = ComparisonFunc::Always; // stencil test function
        };
        // Blend state
        BlendMode blendMode = BlendMode::Opaque;

        //: Per-render-target blend
        struct RenderTargetBlend
        {
            bool blendEnable = false;
            BlendMode blendMode = BlendMode::Opaque;
            uint8_t writeMask = 0xFF;  // Color write mask
        };
        std::vector<RenderTargetBlend> renderTargetBlends;

        // Topology
        PrimitiveTopology topology = PrimitiveTopology::TriangleList;

        //: Render pass compatibility
        RenderPassDesc* renderPass = nullptr;

        //: Descriptor layouts
        std::vector<DescriptorSetLayout> descriptorLayouts;

        std::string debugName;
    };

    // ============================================================================
    //: Fence/Synchronization
    // ============================================================================

    enum class PipelineStage
    {
        Top,
        VertexInput,
        VertexShader,
        PixelShader,
        EarlyDepthTest,
        LateDepthTest,
        ColorOutput,
        ComputeShader,
        Transfer,
        Bottom
    };

    struct FenceDesc
    {
        uint64_t initialValue = 0;
        std::string debugName;
    };

    // ============================================================================
    //: Query Objects (for GPU profiling)
    // ============================================================================

    enum class QueryType
    {
        Timestamp,
        Occlusion,
        PipelineStatistics
    };

    struct QueryDesc
    {
        QueryType type;
        uint32_t count = 1;
        std::string debugName;
    };

    // ============================================================================
    // Enhanced Texture Description
    // ============================================================================

    enum class TextureFormat {
        // Your existing formats
        RGBA8_UNORM,
        RGBA16_FLOAT,
        RGBA32_FLOAT,
        R8_UNORM,
        RG8_UNORM,
        D24_UNORM_S8_UINT,
        BC1_UNORM,
        BC3_UNORM,
        BC5_UNORM,

        //: Additional common formats
        RGBA8_SRGB,           // sRGB color space
        BGRA8_UNORM,          // Common swapchain format
        R16_FLOAT,
        RG16_FLOAT,
        R32_FLOAT,
        RG32_FLOAT,
        RGB32_FLOAT,
        D32_FLOAT,            // High precision depth
        D16_UNORM,            // Low precision depth
        BC6H_UFLOAT,          // HDR compressed
        BC7_UNORM,            // High quality compressed
        R11G11B10_FLOAT       // HDR format
    };

    enum class TextureType {
        Texture2D,
        Texture3D,
        TextureCube,
        Texture2DArray
    };

    struct TextureDesc {
        TextureType type = TextureType::Texture2D;
        TextureFormat format = TextureFormat::RGBA8_UNORM;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t depth = 1;
        uint32_t mipLevels = 1;
        uint32_t arraySize = 1;
        uint32_t sampleCount = 1;     //MSAA support

        bool isRenderTarget = false;
        bool isDepthStencil = false;
        bool allowUnorderedAccess = false;  //For compute shaders
        bool generateMips = false;          //Auto-generate mipmaps

        const void* initialData = nullptr;
        std::string debugName;
    };

    // ============================================================================
    // Enhanced Shader Description
    // ============================================================================

    enum class ShaderStage {
        Vertex,
        Pixel,
        Geometry,
        Hull,
        Domain,
        Compute
    };

    enum class OptimizationLevel {
        None,
        Level1,
        Level2,
        Level3
    };

    struct ShaderMacro
    {
        std::string name;
        std::string value;
    };

    struct ShaderDesc {
        ShaderStage stage;
        std::string sourceCode;
        std::string entryPoint = "main";
        std::vector<ShaderMacro> defines;  // Enhanced from string
        std::string debugName;

        bool enableDebug = false;
        bool enableStrictness = true;
        OptimizationLevel optimizationLevel = OptimizationLevel::Level1;

        //: Include handler for shader includes
        std::function<std::string(const std::string&)> includeHandler;
    };

    // ============================================================================
    // Enhanced Buffer Description
    // ============================================================================

    struct BufferDesc {
        BufferType type = BufferType::Vertex;
        BufferUsage usage = BufferUsage::Static;
        uint32_t size = 0;
        uint32_t stride = 0;

        //: Additional flags
        bool allowUnorderedAccess = false;  // For compute shaders
        bool indirectArgs = false;          // For indirect drawing

        const void* initialData = nullptr;
        std::string debugName;
    };

    // ============================================================================
    // Vertex Attributes (Enhanced)
    // ============================================================================

    enum class VertexAttributeType
    {
        Position,
        Normal,
        Tangent,
        Bitangent,
        TexCoord0,
        TexCoord1,
        TexCoord2,
        TexCoord3,
        Color0,
        Color1,
        BlendIndices,
        BlendWeights,
        Custom
    };

    enum class DataFormat
    {
        Float,
        Float2,
        Float3,
        Float4,
        Int,
        Int2,
        Int3,
        Int4,
        UInt,     
        UInt2,    
        UInt3,    
        UInt4,    
        UByte4,
        UByte4N,
        Short2,
        Short2N,
        Short4,
        Short4N,
        Half2,
        Half4
    };

    struct VertexAttribute
    {
        VertexAttributeType Type;
        DataFormat Format;
        std::string SemanticName;
        uint32_t SemanticIndex;
        uint32_t Offset;
        uint32_t Slot;
        bool PerInstance;

        VertexAttribute() = default;
        VertexAttribute(VertexAttributeType attrType,
            DataFormat dataFormat,
            const std::string& semantic = "",
            uint32_t semIndex = 0,
            uint32_t inputSlot = 0,
            bool instanceData = false)
            : Type(attrType)
            , Format(dataFormat)
            , SemanticName(semantic.empty() ? GetDefaultSemanticName(attrType) : semantic)
            , SemanticIndex(semIndex)
            , Offset(0) // Will be calculated by layout
            , Slot(inputSlot)
            , PerInstance(instanceData)
        {
        }

        uint32_t GetSize() const
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
            case DataFormat::UInt:      return sizeof(uint32_t);
            case DataFormat::UInt2:     return sizeof(uint32_t) * 2;
            case DataFormat::UInt3:     return sizeof(uint32_t) * 3;
            case DataFormat::UInt4:     return sizeof(uint32_t) * 4;
            case DataFormat::UByte4:    return sizeof(uint8_t) * 4;
            case DataFormat::UByte4N:   return sizeof(uint8_t) * 4;
            case DataFormat::Short2:    return sizeof(int16_t) * 2;
            case DataFormat::Short2N:   return sizeof(int16_t) * 2;
            case DataFormat::Short4:    return sizeof(int16_t) * 4;
            case DataFormat::Short4N:   return sizeof(int16_t) * 4;
            case DataFormat::Half2:     return sizeof(uint16_t) * 2;
            case DataFormat::Half4:     return sizeof(uint16_t) * 4;
            default:                    return 0;
            }
        }

        static std::string GetDefaultSemanticName(VertexAttributeType type)
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
    };

    // ============================================================================
    // Other Existing Types
    // ============================================================================

    enum class PrimitiveTopology {
        TriangleList,
        TriangleStrip,
        LineList,
        LineStrip,
        PointList,
        TriangleListAdj,
        TriangleStripAdj,
        LineListAdj,
        LineStripAdj,
        PatchList      //: For tessellation
    };

    enum class BlendMode {
        Opaque,
        AlphaBlend,
        Additive,
        Multiply,
        Premultiplied  //Premultiplied alpha
    };

    enum class DepthTestMode {
        None,
        Less,
        LessEqual,
        Greater,
        GreaterEqual,  
        Equal,
        NotEqual,      
        Always         
    };

    enum class CullMode {
        None,
        Front,
        Back
    };

    struct RasterizerState
    {
        CullMode cullMode = CullMode::Back;
        bool wireframe = false;
        bool frontCounterClockwise = false;

        //: Advanced rasterizer state
        int32_t depthBias = 0;
        float depthBiasClamp = 0.0f;
        float slopeScaledDepthBias = 0.0f;
        bool depthClipEnable = true;
        bool scissorEnable = false;
        bool multisampleEnable = false;
        bool antialiasedLineEnable = false;
    };

    struct ViewportDesc {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        float minDepth = 0.0f;
        float maxDepth = 1.0f;
    };

    struct DeviceCapabilities {
        uint32_t maxTextureSize = 0;
        uint32_t maxAnisotropy = 0;
        uint32_t maxConstantBufferSize = 0;
        uint32_t maxComputeWorkGroupSize[3] = { 0, 0, 0 };  
        uint32_t maxComputeWorkGroupCount[3] = { 0, 0, 0 }; 

        bool supportsCompute = false;
        bool supportsGeometryShaders = false;
        bool supportsTessellation = false;
        bool supportsBindless = false;
        bool supportsRayTracing = false;     
        bool supportsMeshShaders = false;    
        bool supportsVariableRateShading = false;  

        std::string deviceName;
        std::string driverVersion;  
    };

    enum class ResourceState {
        Undefined,
        VertexBuffer,
        IndexBuffer,
        ConstantBuffer,
        ShaderResource,
        UnorderedAccess,
        RenderTarget,
        DepthWrite,
        DepthRead,
        Present,
        CopySource,
        CopyDest,
        ResolveSrc,    
        ResolveDst,    
        General        
    };

} 