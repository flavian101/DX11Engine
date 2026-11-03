#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace DXEngine
{
    class RenderPass;
    class FrameContext;

    // ==================================================================
    // RenderGraph: an ORDERED list of passes.
    //
    //  * Default order = the order you call AddPass().      (simple, predictable)
    //  * DependsOn(a, b) forces "b runs before a".          (only needed if you add
    //    passes out of order; ties are always broken by insertion order, so the
    //    result is deterministic.)
    //
    // This is deliberately NOT a resource-tracking frame graph. For a forward
    // renderer with ~5 passes an ordered list is easier to read and debug.
    // ==================================================================
    class RenderGraph
    {
    public:
        // Returns the typed pass so the caller can keep a handle (e.g. to set the clear colour).
        // Returns nullptr if a pass with that name already exists.
        template<typename T, typename... Args>
        std::shared_ptr<T> AddPass(const std::string& name, Args&&... args)
        {
            auto pass = std::make_shared<T>(std::forward<Args>(args)...);
            return AddNode(name, pass) ? pass : nullptr;
        }

        RenderGraph& DependsOn(const std::string& passName, const std::string& mustRunBefore);
        void RemovePass(const std::string& name);
        void EnablePass(const std::string& name, bool enable = true);

        bool Compile();                       // validate + compute execution order
        bool Initialize(FrameContext& ctx);   // call after Compile(): runs every pass's Initialize
        void Execute(FrameContext& ctx);      // Prepare -> Execute -> Finalize per enabled pass
        void OnResize(uint32_t width, uint32_t height);
        void Shutdown();

        bool HasPass(const std::string& name) const;
        std::shared_ptr<RenderPass> GetPass(const std::string& name) const;
        size_t GetPassCount() const { return m_Nodes.size(); }
        std::string GetDebugInfo() const;

    private:
        struct PassNode
        {
            std::string                 name;
            std::shared_ptr<RenderPass> pass;
            std::vector<std::string>    runAfter;   // names that must execute before this one
        };

        bool   AddNode(const std::string& name, std::shared_ptr<RenderPass> pass);
        int    FindIndex(const std::string& name) const;

        std::vector<PassNode> m_Nodes;      // insertion order
        std::vector<size_t>   m_Order;      // indices into m_Nodes, execution order
        bool m_Compiled    = false;
        bool m_Initialized = false;
    };
}
