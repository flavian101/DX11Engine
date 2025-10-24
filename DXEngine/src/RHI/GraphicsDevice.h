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
	class ISampler;
	class IFence;
	class IQuery;
	class IRenderPass;

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

		//Partial map for largeBuffers
		virtual void* MapRange(uint32_t offset, uint32_t size) = 0;

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
		virtual uint32_t GetArraySize() const = 0;
		virtual uint32_t GetSampleCount() const = 0;

		//data Operations
		virtual bool Update(const void* data, uint32_t mipLevel = 0, uint32_t arraySlice = 0) = 0;
		virtual bool GenerateMips() = 0;

		//Read back texture data
		virtual bool ReadPixels(void* outData, uint32_t mipLevel = 0, uint32_t arraySlice = 0) = 0;
		
		//Binding
		virtual void* GetNativeHandle()const = 0; //platform-specific handle
	};

	class ISampler : public IGraphicsResource
	{
	public:
		virtual ~ISampler() = default;

		virtual const SamplerDesc& GetDesc() const = 0;
		virtual void* GetNativeHandle() const = 0;
	};

	class IShader : public IGraphicsResource
	{
	public:
		virtual ~IShader() = default;

		virtual ShaderStage GetStage() const = 0;
		virtual const void* GetBytecode() const = 0;
		virtual size_t GetBytecodeSize() const = 0;
		virtual bool IsCompiled()const = 0;

		// Shader reflection
		struct ReflectionData
		{
			std::vector<DescriptorBinding> bindings;
			uint32_t constantBufferSize = 0;
			std::string entryPoint;
		};
		virtual const ReflectionData& GetReflection() const = 0;

	};

	//Render pipeline state Object
	class IPipeline : public IGraphicsResource
	{
	public:
		virtual ~IPipeline() = default;

		virtual const PipelineDesc& GetDesc() const = 0;
		virtual void* GetNativeHandle()const = 0;
	};

	//Render Pass interface
	class IRenderPass : public IGraphicsResource
	{
	public:
		virtual ~IRenderPass() = default;

		virtual const RenderPassDesc& GetDesc() const = 0;
		virtual void* GetNativeHandle() const = 0;
	};

	//Fence/Synchronization Interface
	class IFence : public IGraphicsResource
	{
	public:
		virtual ~IFence() = default;

		virtual void Signal(uint64_t value) = 0;
		virtual void Wait(uint64_t value, uint64_t timeout = UINT64_MAX) = 0;
		virtual uint64_t GetCompletedValue() const = 0;
	};

	//Query Interface (for GPU profiling)
	class IQuery : public IGraphicsResource
	{
	public:
		virtual ~IQuery() = default;

		virtual QueryType GetType() const = 0;
		virtual bool GetData(void* outData, size_t dataSize) = 0;
		virtual bool IsReady() const = 0;
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

		virtual void Reset() = 0;

		//Binding Operations
		virtual void SetPipeline(IPipeline* pipeline) = 0;
		virtual void SetVertexBuffer(IBuffer* buffer, uint32_t slot = 0, uint32_t offset = 0) = 0;
		virtual void SetIndexBuffer(IBuffer* buffer, uint32_t slot = 0, uint32_t offset = 0) = 0;
		virtual void SetConstantBuffer(IBuffer* buffer, uint32_t slot = 0, ShaderStage stage = ShaderStage::Vertex) = 0;
		virtual void SetTexture(ITexture* texture, uint32_t slot = 0, ShaderStage stage = ShaderStage::Pixel) = 0;
		virtual void SetSampler(ISampler* sampler, uint32_t slot = 0, ShaderStage stage = ShaderStage::Pixel) = 0;

		//Multiple Resource binding
		virtual void SetVertexBuffers(IBuffer** buffers, uint32_t count, uint32_t startSlot = 0) = 0;
		virtual void SetTextures(ITexture** textures, uint32_t count, uint32_t startSlot = 0, ShaderStage stage = ShaderStage::Pixel) = 0;

		//Drawing Commands
		virtual void Draw(uint32_t vertexCount, uint32_t startVertex = 0) = 0;
		virtual void DrawIndexed(uint32_t indexCount, uint32_t startIndex = 0, int32_t baseIndex = 0) = 0;
		virtual void DrawInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertex = 0, uint32_t startInstance = 0) = 0;
		virtual void DrawIndexedInstanced(uint32_t indexCount,
			uint32_t instanceCount, uint32_t startIndex,
			int32_t baseVertex = 0, uint32_t startInstance = 0) = 0;

		//Indirect drawing
		virtual void DrawIndirect(IBuffer* argsBuffer, uint32_t offset = 0) = 0;
		virtual void DrawIndexedIndirect(IBuffer* argsBuffer, uint32_t offset = 0) = 0;

		//render Targets
		virtual void SetRenderTarget(ITexture* colorTarget, ITexture* depthTarget = nullptr);
		virtual void SetRenderTargets(ITexture** colorTargets, uint32_t count, ITexture* depthTarget = nullptr) = 0;
		virtual void ClearRenderTarget(ITexture* target, float r, float g, float b, float a) = 0;
		virtual void ClearDepthStencil(ITexture* target, float depth, uint8_t stencil = 0) = 0;

		//Render pass support
		virtual void BeginRenderPass(IRenderPass* renderPass, ITexture** targets, uint32_t targetCount) = 0;
		virtual void EndRenderPass() = 0;

		//viewPort and scissors
		virtual void SetViewport(const ViewportDesc& viewport) = 0;
		virtual void SetScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;

		//Resource barriers/transitions (for D3D12/Vulkan)
		virtual void TransitionBarrier(ITexture* texture, ResourceState before, ResourceState after) = 0;
		virtual void TransitionBarrier(IBuffer* buffer, ResourceState before, ResourceState after) = 0;

		//Copy operations
		virtual void CopyBuffer(IBuffer* src, IBuffer* dst, uint32_t srcOffset, uint32_t dstOffset, uint32_t size) = 0;
		virtual void CopyTexture(ITexture* src, ITexture* dst) = 0;
		virtual void CopyBufferToTexture(IBuffer* src, ITexture* dst, uint32_t mipLevel = 0, uint32_t arraySlice = 0) = 0;
		virtual void CopyTextureToBuffer(ITexture* src, IBuffer* dst, uint32_t mipLevel = 0, uint32_t arraySlice = 0) = 0;

		//Compute shader dispatch
		virtual void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;
		virtual void DispatchIndirect(IBuffer* argsBuffer, uint32_t offset = 0) = 0;

		//Query operations
		virtual void BeginQuery(IQuery* query, uint32_t index = 0) = 0;
		virtual void EndQuery(IQuery* query, uint32_t index = 0) = 0;
		virtual void WriteTimestamp(IQuery* query, PipelineStage stage, uint32_t index = 0) = 0;

		//Push constants (for small, frequent updates)
		virtual void PushConstants(const void* data, uint32_t size, uint32_t offset = 0) = 0;

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
		virtual std::shared_ptr<ISampler> CreateSampler(const SamplerDesc& desc) = 0;
		virtual std::shared_ptr<IRenderPass> CreateRenderPass(const RenderPassDesc& desc) = 0;
		virtual std::shared_ptr<IFence> CreateFence(const FenceDesc& desc) = 0;
		virtual std::shared_ptr<IQuery> CreateQuery(const QueryDesc& desc) = 0;


		//command submission
		virtual std::shared_ptr<ICommandBuffer> CreateCommandBuffer() = 0;
		virtual void Submit(ICommandBuffer* cmd) = 0;

		//Batch submission with synchronization
		virtual void Submit(ICommandBuffer** cmds, uint32_t count, IFence* signalFence = nullptr) = 0;

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
		virtual uint32_t GetCurrentBackBufferIndex() const = 0; //For multi-buffering

		// Memory management
		struct MemoryStats
		{
			size_t totalAllocated;
			size_t totalUsed;
			size_t bufferMemory;
			size_t textureMemory;
			uint32_t bufferCount;
			uint32_t textureCount;
		};
		virtual MemoryStats GetMemoryStats() const = 0;

		//Performance queries
		virtual uint64_t GetTimestampFrequency() const = 0;  // For converting timestamps to time

	};

	class GraphicsDeviceFactory
	{
	public:
		static std::unique_ptr<IGraphicsDevice> Create(GraphicsAPI api);

		//Query available APIs on the current platform
		static std::vector<GraphicsAPI> GetAvailableAPIs();
		static bool IsAPISupported(GraphicsAPI api);
	};


	class CommandPool
	{
	public:
		CommandPool(IGraphicsDevice* device, size_t initialSize = 4)
			: m_Device(device)
		{
			m_AvailableBuffers.reserve(initialSize);
			for (size_t i = 0; i < initialSize; ++i)
			{
				m_AvailableBuffers.push_back(device->CreateCommandBuffer());
			}
		}

		std::shared_ptr<ICommandBuffer> Allocate()
		{
			if (m_AvailableBuffers.empty())
			{
				return m_Device->CreateCommandBuffer();
			}

			auto cmd = m_AvailableBuffers.back();
			m_AvailableBuffers.pop_back();
			m_InUseBuffers.push_back(cmd);
			return cmd;
		}

		void Reset()
		{
			for (auto& cmd : m_InUseBuffers)
			{
				cmd->Reset();
				m_AvailableBuffers.push_back(cmd);
			}
			m_InUseBuffers.clear();
		}

	private:
		IGraphicsDevice* m_Device;
		std::vector<std::shared_ptr<ICommandBuffer>> m_AvailableBuffers;
		std::vector<std::shared_ptr<ICommandBuffer>> m_InUseBuffers;
	};

	// ============================================================================
	// NEW: Frame Graph (for automatic resource management and optimization)
	// ============================================================================

	class FrameGraph
	{
	public:
		struct PassDesc
		{
			std::string name;
			std::function<void(ICommandBuffer*)> execute;
			std::vector<ITexture*> colorOutputs;
			ITexture* depthOutput = nullptr;
			std::vector<ITexture*> textureInputs;
			std::vector<IBuffer*> bufferInputs;
		};

		void AddPass(const PassDesc& pass);
		void Compile();
		void Execute(ICommandBuffer* cmd);
		void Reset();

	private:
		std::vector<PassDesc> m_Passes;
		bool m_IsCompiled = false;
	};
}
