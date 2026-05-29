#pragma once
#include <memory>
#include <string>


namespace DXEngine::Rendering
{
	class FrameContext;
	//Render Pass- Base interface. One pass, one responsibility.

	//lifecycle per frame
	//Prepare() - CPU work: uploads constants, build command list, etc.
	//Execute() - GPU work: issue draw / dispatch calls 
	//Finalize() - Post-frame: release transient resources, read-back 

	//Passes communicate exclusivly through FrameContext (blackBoard).
	//They must NOT hold raw pointers to other passes   

	class RenderPass
	{
	public:
		virtual ~RenderPass() = default;

		//called once after the graph is compiled and before any frames render
		//use to allocate persistent GPU resources (constant buffers, etc)
		virtual bool Initialize(FrameContext& ctx) { return true; }

		//CPU work - runs before Execute. upload per-frrame data here.
		virtual void Prepare(FrameContext& ctx) {}

		//GPU work - issues draw/ dispatch calls here
		virtual void Execute(FrameContext& ctx) = 0;

		//post-frame cleanup. releases transient resources, signal fences, etc
		virtual void Finalize(FrameContext& ctx){}

		//Free all GPU resources. called from RenderGraph::Clear().
		virtual void Shutdown(){}

		//notify the pass that the SwapChain was resized
		virtual void OnResize(uint32_t width, uint32_t height) {}

		// Human-readable identifier used in logging and debug UI.
		virtual std::string GetName() const = 0;

		// Optional per-pass stats string appended to renderer debug output.
		virtual std::string GetStatistics() const { return ""; }

		bool IsEnabled() const { return m_Enabled; }
		void SetEnabled(bool enabled) { m_Enabled = enabled; }

	protected:
		bool m_Enabled = true;
	};
}
