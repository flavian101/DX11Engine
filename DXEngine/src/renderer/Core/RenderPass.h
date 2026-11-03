#pragma once
#include <cstdint>
#include <memory>
#include <string>

namespace DXEngine
{
    class FrameContext;

    // ==================================================================
    // RenderPass: GPU work. One pass, one responsibility.
    //
    // Lifecycle
    //   Initialize()  once, after the graph is compiled (create persistent GPU objects)
    //   Prepare()     every frame, CPU side (optional)
    //   Execute()     every frame, issues the GPU commands
    //   Finalize()    every frame, cleanup (optional)
    //   Shutdown()    once, release GPU objects
    //
    // Passes talk ONLY through FrameContext. They never hold another pass.
    // Each pass header documents "Reads:" and "Writes:" in its class comment.
    // ==================================================================
    class RenderPass
    {
    public:
        virtual ~RenderPass() = default;

        virtual bool Initialize(FrameContext& ctx) { return true; }
        virtual void Prepare(FrameContext& ctx)    {}
        virtual void Execute(FrameContext& ctx) = 0;
        virtual void Finalize(FrameContext& ctx)   {}
        virtual void Shutdown()                    {}
        virtual void OnResize(uint32_t width, uint32_t height) {}

        virtual std::string GetName() const = 0;

        // ONE enable flag (the graph used to keep a second, out-of-sync copy)
        bool IsEnabled() const { return m_Enabled; }
        void SetEnabled(bool enabled) { m_Enabled = enabled; }

    private:
        bool m_Enabled = true;
    };
}
