#pragma once
#include "GraphicsTypes.h"
#include <memory>
#include <vector>

namespace DXEngine::RHI
{
	class IBuffer;
	class ITexture;
	class IPipeline;
	class ICommandBuffer;

	//Base class for all GPU resources
	class IGraphicsResource
	{
	public:
		virtual ~IGraphicsResource() = default;

		virtual const std::string& GetDebugName() const = 0;
		virtual void SetDebugName(const std::string& name) = 0;
		virtual uint64_t GetGPUHandle()const = 0;
		virtual size_t GetMemoryUsage()const = 0;
		virtual bool IsValid()const = 0;
	};


	//Buffer Inteface
	class IBuffer : public IGraphicsResource
	{
	public:
		virtual ~IBuffer() = default;

		//Buffer Properties
		virtual BufferType GetType()const = 0;
		virtual uint32_t GetSize() const = 0;
		virtual uint32_t GetStride()const = 0;

		//Data operations
		virtual bool Update(const void* data, uint32_t size, uint32_t offset = 0) = 0;
		virtual bool Read(void* outData, uint32_t size, uint32_t offset = 0) = 0;


		//mapping (for dynamic buffers)
		virtual void* Map() = 0;
		virtual void UnMap() = 0;
	};

	class ITexture : public IGraphicsResource
	{
	public:
		virtual ~ITexture() = default;

		//Texture properties
		virtual TextureType GetType() const = 0;
		virtual TextureFormat GetFormat() const = 0;
		virtual uint32_t GetWidth()const = 0;
		virtual uint32_t GetHeight()const = 0;
		virtual uint32_t GetDepth()const = 0;
		virtual uint32_t GetMipLevels()const = 0;

		//data Operations
		virtual bool Update(const void* data, uint32_t mipLevel = 0, uint32_t arraySlice = 0) = 0;
		virtual bool GenerateMips() = 0;
		
		//Binding
		virtual void* GetNativeHandle()const = 0; //platform-specific handle
	};

	class IShader : public IGraphicsResource
	{
	public:
		virtual ~IShader() = default;

		virtual ShaderStage GetStage() const = 0;
		virtual const void* GetBytecode() const = 0;
		virtual size_t GetBytecodeSize() const = 0;
		virtual bool IsCompiled()const = 0;
	};

	//Render pipeline state Object
	class IPipeline : public IGraphicsResource
	{
	public:
		virtual ~IPipeline() = default;

		virtual const PipelineDesc& GetDesc() const = 0;
		virtual void* GetNativeHandle()const = 0;
	};

	//command Recording interface
	class ICommandBuffer
	{
	public:
		virtual ~ICommandBuffer() = default;

		//Recording state
		virtual void Begin() = 0;
		virtual void End() = 0;
		virtual bool IsRecording()const = 0;

		//Binding Operations
		virtual void SetPipeLine(IPipeline* pipeline) = 0;
		virtual void SetVertexBuffer(IBuffer* buffer, uint32_t slot = 0, uint32_t offset = 0) = 0;
		virtual void SetIndexBuffer(IBuffer* buffer, uint32_t slot = 0, uint32_t offset = 0) = 0;
		virtual void SetConstantBuffer(IBuffer* buffer, uint32_t slot = 0, ShaderStage stage = ShaderStage::Vertex) = 0;
		virtual void SetTexture(ITexture* texture, uint32_t slot = 0, ShaderStage stage = ShaderStage::Pixel) = 0;


		//Drawing Commands
		virtual void Draw(uint32_t vertexCount, uint32_t startVertex = 0) = 0;
		virtual void DrawIndexed(uint32_t indexCount, uint32_t startIndex = 0, int32_t baseIndex = 0) = 0;
		virtual void DrawInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertex = 0, uint32_t startInstance = 0) = 0;
		virtual void DrawIndexedInstanced(uint32_t indexCount, 
			uint32_t instanceCount, uint32_t startIndex,
			uint32_t baseVertex = 0, uint32_t startInstance = 0) = 0;

		//render Targets
		virtual void SetRenderTarget(ITexture* colorTarget, ITexture* depthTarget = nullptr);
		virtual void ClearRenderTarget(ITexture* target, float r, float g, float b, float a) = 0;
		virtual void ClearDepthStencil(ITexture* target, float depth, uint8_t stencil = 0) = 0;


		//viewPort and scissors
		virtual void SetViewport(const ViewportDesc& viewport) = 0;
		virtual void SetScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
	};

	//Main Device
	class IGraphicsDevice
	{
	public:
		virtual ~IGraphicsDevice() = default;

		//Initialzation
		virtual bool Initialize(void* windowHandle, uint32_t width, uint32_t height) = 0;
		virtual void Shutdown() = 0;

		//Resource creation
		virtual std::shared_ptr<IBuffer> CreateBuffer(const BufferDesc& desc) = 0;
		virtual std::shared_ptr<ITexture> CreateTexture(const TextureDesc& desc) = 0;
		virtual std::shared_ptr<IShader> CreateShader(const ShaderDesc& desc) = 0;
		virtual std::shared_ptr<IPipeline> CreatePipeline(const PipelineDesc& desc) = 0;


		//command submission
		virtual std::shared_ptr<ICommandBuffer> CreateCommandBuffer() = 0;
		virtual void Submit(ICommandBuffer* cmd) = 0;
		virtual void Present() = 0;
		virtual void WaitIdle() = 0;

		//Device Info
		virtual GraphicsAPI GetAPI() const = 0;
		virtual const DeviceCapabilities& GetCapabilities() const = 0;
		virtual void* GetnativeDevice() const = 0;

		//SwapChain Management
		virtual void Resize(uint32_t width, uint32_t height) = 0;
		virtual ITexture* GetBackBuffer() = 0;
		virtual uint32_t GetBackBufferWidth() const = 0;
		virtual uint32_t GetBackBufferHeight() const = 0;

	};

	class GraphicsDeviceFactory
	{
	public:
		static std::unique_ptr<IGraphicsDevice> Create(GraphicsAPI api);
	};
}
