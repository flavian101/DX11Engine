#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace DXEngine::RHI
{
	enum class GraphicsAPI
	{
		DirectX11,
		DirectX12,
		Vulcan,
		OpenGl
	};

	enum class BufferType
	{
		Vertex,
		Index,
		Uniform,      // Constant buffers
		Storage,      // Structured buffers
		Staging       // CPU-accessible

	};

	enum class BufferUsage
	{
		Static,       // Created once, never updated
		Dynamic,      // Updated frequently (per-frame)
		Stream        // Updated every frame, discarded after use

	};

	enum class TextureFormat {
		RGBA8_UNORM,
		RGBA16_FLOAT,
		RGBA32_FLOAT,
		R8_UNORM,
		RG8_UNORM,
		D24_UNORM_S8_UINT,  // Depth-stencil
		BC1_UNORM,          // Compressed
		BC3_UNORM,
		BC5_UNORM
	};

	enum class TextureType {
		Texture2D,
		Texture3D,
		TextureCube,
		Texture2DArray
	};

	enum class ShaderStage {
		Vertex,
		Pixel,
		Compute,
		Geometry,
		Hull,
		Domain
	};

	enum class OptimizationLevel {
		None,      // No optimization (debug builds)
		Level1,    // Basic optimization
		Level2,    // Moderate optimization
		Level3     // Maximum optimization (release builds)
	};

	struct ShaderDesc {
		ShaderStage stage;
		std::string sourceCode;
		std::string entryPoint = "main";
		std::string defines;         // Preprocessor defines
		std::string debugName;

		// Compilation flags
		bool enableDebug = false;            // Include debug info
		bool enableStrictness = true;        // Strict compilation mode
		OptimizationLevel optimizationLevel = OptimizationLevel::Level1;
	};


	enum class PrimitiveTopology {
		TriangleList,
		TriangleStrip,
		LineList,
		LineStrip,
		PointList,
		TriangleListAdj,
		TriangleStripAdj,
		LineListAdj,
		LineStripAdj
	};

	enum class BlendMode {
		Opaque,
		AlphaBlend,
		Additive,
		Multiply
	};

	enum class DepthTestMode {
		None,
		Less,
		LessEqual,
		Greater,
		Equal
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
	};

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
	enum class DataFormat // format specification
	{
		Float,
		Float2,
		Float3,
		Float4,
		Int,
		Int2,
		Int3,
		Int4,
		UByte4,
		UByte4N,    // Normalized
		Short2,
		Short2N,    // Normalized
		Short4,
		Short4N,    // Normalized
		Half2,
		Half4
	};

	struct VertexAttribute
	{
		RHI::VertexAttributeType Type;
		RHI::DataFormat Format;
		std::string SemanticName;
		uint32_t SemanticIndex;
		uint32_t Offset;
		uint32_t Slot; //mutiple buffers
		bool PerInstance;

		RHI::VertexAttribute() = default;
		RHI::VertexAttribute(RHI::VertexAttributeType attrType,
			RHI::DataFormat DataFormat,
			const std::string& semantic = "",
			uint32_t semIndex = 0,
			uint32_t inputSlot = 0,
			bool instanceData = false)
			: Type(attrType)
			, Format(DataFormat)
			, SemanticName(semantic.empty() ? GetDefaultSemanticName(attrType) : semantic)
			, SemanticIndex(semIndex)
			, Offset(0)  // Will be calculated by layout
			, Slot(inputSlot)
			, PerInstance(instanceData) {
		}

		uint32_t GetSize()const
		{
			switch (Format)
			{
			case RHI::DataFormat::Float:     return sizeof(float);
			case RHI::DataFormat::Float2:    return sizeof(float) * 2;
			case RHI::DataFormat::Float3:    return sizeof(float) * 3;
			case RHI::DataFormat::Float4:    return sizeof(float) * 4;
			case RHI::DataFormat::Int:       return sizeof(int32_t);
			case RHI::DataFormat::Int2:      return sizeof(int32_t) * 2;
			case RHI::DataFormat::Int3:      return sizeof(int32_t) * 3;
			case RHI::DataFormat::Int4:      return sizeof(int32_t) * 4;
			case RHI::DataFormat::UByte4:    return sizeof(uint8_t) * 4;
			case RHI::DataFormat::UByte4N:   return sizeof(uint8_t) * 4;
			case RHI::DataFormat::Short2:    return sizeof(int16_t) * 2;
			case RHI::DataFormat::Short2N:   return sizeof(int16_t) * 2;
			case RHI::DataFormat::Short4:    return sizeof(int16_t) * 4;
			case RHI::DataFormat::Short4N:   return sizeof(int16_t) * 4;
			case RHI::DataFormat::Half2:     return sizeof(uint16_t) * 2; // Half precision
			case RHI::DataFormat::Half4:     return sizeof(uint16_t) * 4; // Half precision
			default:                    return 0;
			}
		}
		static std::string GetDefaultSemanticName(RHI::VertexAttributeType type)
		{
			switch (type)
			{
			case RHI::VertexAttributeType::Position:     return "POSITION";
			case RHI::VertexAttributeType::Normal:       return "NORMAL";
			case RHI::VertexAttributeType::Tangent:      return "TANGENT";
			case RHI::VertexAttributeType::Bitangent:    return "BITANGENT";
			case RHI::VertexAttributeType::TexCoord0:    return "TEXCOORD";
			case RHI::VertexAttributeType::TexCoord1:    return "TEXCOORD";
			case RHI::VertexAttributeType::TexCoord2:    return "TEXCOORD";
			case RHI::VertexAttributeType::TexCoord3:    return "TEXCOORD";
			case RHI::VertexAttributeType::Color0:       return "COLOR";
			case RHI::VertexAttributeType::Color1:       return "COLOR";
			case RHI::VertexAttributeType::BlendIndices: return "BLENDINDICES";
			case RHI::VertexAttributeType::BlendWeights: return "BLENDWEIGHT";
			case RHI::VertexAttributeType::Custom:       return "CUSTOM";
			default:                                return "UNKNOWN";
			}
		}
	};


	//Descriptors Structures
	struct BufferDesc {
		BufferType type = BufferType::Vertex;
		BufferUsage usage = BufferUsage::Static;
		uint32_t size = 0;           // Size in bytes
		uint32_t stride = 0;         // For structured buffers
		const void* initialData = nullptr;
		std::string debugName;
	};

	struct TextureDesc {
		TextureType type = TextureType::Texture2D;
		TextureFormat format = TextureFormat::RGBA8_UNORM;
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t depth = 1;          // For 3D textures
		uint32_t mipLevels = 1;
		uint32_t arraySize = 1;
		bool isRenderTarget = false;
		bool isDepthStencil = false;
		const void* initialData = nullptr;
		std::string debugName;
	};



	struct PipelineDesc
	{
		//Shader Stages
		class IShader* vertexShader = nullptr;
		class IShader* pixelShader = nullptr;
		class IShader* computeShader = nullptr;
		class IShader* geometryShader = nullptr;

		//Input Layout(vertex format)
		std::vector<VertexAttribute> inputLayout;

		//Rasterizer State
		RasterizerState rasterizer;

		// Depth-stencil state
		DepthTestMode depthTest = DepthTestMode::Less;
		bool depthWrite = true;

		// Blend state
		BlendMode blendMode = BlendMode::Opaque;

		// Topology
		PrimitiveTopology topology = PrimitiveTopology::TriangleList;

		std::string debugName;
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
		bool supportsCompute = false;
		bool supportsGeometryShaders = false;
		bool supportsTessellation = false;
		bool supportsBindless = false;
		std::string deviceName;
	};

	// ============================================================================
	// Resource States (for D3D12/Vulkan barriers)
	// ============================================================================

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
		CopyDest
	};



}