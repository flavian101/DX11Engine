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

	enum class PrimitiveTopology {
		TriangleList,
		TriangleStrip,
		LineList,
		PointList
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

	struct ShaderDesc {
		ShaderStage stage;
		std::string sourceCode;
		std::string entryPoint = "main";
		std::string defines;         // Preprocessor defines
		std::string debugName;
	};

	struct PipelineDesc
	{
		//Shader Stages
		class IShader* vertexShader = nullptr;
		class IShader* pixelShader = nullptr;
		class IShader* computeShader = nullptr;
		class IShader* geometryShader = nullptr;

		//Input Layout(vertex formart)
		struct VertexInputElement {
			std::string semantic;
			uint32_t semanticIndex = 0;
			TextureFormat format;
			uint32_t inputSlot = 0;
			uint32_t offset = 0;
		};
		std::vector<VertexInputElement> inputLayout;

		//Rasterizer State
		CullMode cullMode = CullMode::Back;
		bool wireframe = false;

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