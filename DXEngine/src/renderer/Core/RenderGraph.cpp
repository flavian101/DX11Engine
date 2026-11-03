#include "dxpch.h"
#include "RenderGraph.h"
#include "RenderPass.h"
#include "FrameContext.h"
#include <sstream>

namespace DXEngine
{
    static void Log(const std::string& msg) { OutputDebugStringA((msg + "\n").c_str()); }

    int RenderGraph::FindIndex(const std::string& name) const
    {
        for (size_t i = 0; i < m_Nodes.size(); ++i)
            if (m_Nodes[i].name == name) return static_cast<int>(i);
        return -1;
    }

    bool RenderGraph::AddNode(const std::string& name, std::shared_ptr<RenderPass> pass)
    {
        if (!pass || FindIndex(name) >= 0)
        {
            Log("WARNING: RenderGraph::AddPass - null pass or duplicate name: " + name);
            return false;
        }
        m_Nodes.push_back({ name, std::move(pass), {} });
        m_Compiled = m_Initialized = false;
        return true;
    }

    RenderGraph& RenderGraph::DependsOn(const std::string& passName, const std::string& mustRunBefore)
    {
        const int i = FindIndex(passName);
        if (i < 0) { Log("WARNING: RenderGraph::DependsOn - unknown pass: " + passName); return *this; }
        m_Nodes[i].runAfter.push_back(mustRunBefore);
        m_Compiled = false;
        return *this;
    }

    void RenderGraph::RemovePass(const std::string& name)
    {
        const int i = FindIndex(name);
        if (i < 0) return;
        m_Nodes[i].pass->Shutdown();
        m_Nodes.erase(m_Nodes.begin() + i);
        m_Compiled = false;
    }

    void RenderGraph::EnablePass(const std::string& name, bool enable)
    {
        const int i = FindIndex(name);
        if (i >= 0) m_Nodes[i].pass->SetEnabled(enable);   // the single source of truth
    }

    bool RenderGraph::Compile()
    {
        // 1) every dependency must exist
        for (const auto& n : m_Nodes)
            for (const auto& dep : n.runAfter)
                if (FindIndex(dep) < 0)
                {
                    Log("ERROR: RenderGraph::Compile - '" + n.name + "' depends on unknown pass '" + dep + "'");
                    return false;
                }

        // 2) stable topological sort.
        //    Repeatedly take the LOWEST-INDEX pass whose dependencies are all placed.
        //    Lowest-index-first means: with no DependsOn calls, order == insertion order.
        const size_t count = m_Nodes.size();
        std::vector<bool> placed(count, false);
        m_Order.clear();

        while (m_Order.size() < count)
        {
            bool progressed = false;
            for (size_t i = 0; i < count && !progressed; ++i)
            {
                if (placed[i]) continue;

                bool ready = true;
                for (const auto& dep : m_Nodes[i].runAfter)
                    if (!placed[FindIndex(dep)]) { ready = false; break; }

                if (ready)
                {
                    placed[i] = true;
                    m_Order.push_back(i);
                    progressed = true;
                }
            }
            if (!progressed)
            {
                Log("ERROR: RenderGraph::Compile - cycle in pass dependencies");
                m_Order.clear();
                return false;
            }
        }

        m_Compiled = true;
        Log("RenderGraph compiled:\n" + GetDebugInfo());
        return true;
    }

    bool RenderGraph::Initialize(FrameContext& ctx)
    {
        if (!m_Compiled) { Log("ERROR: RenderGraph::Initialize - call Compile() first"); return false; }

        for (size_t idx : m_Order)
        {
            auto& n = m_Nodes[idx];
            if (!n.pass->Initialize(ctx))
            {
                Log("ERROR: pass failed to initialize: " + n.name);
                return false;
            }
        }
        m_Initialized = true;
        return true;
    }

    void RenderGraph::Execute(FrameContext& ctx)
    {
        if (!m_Compiled || !m_Initialized) return;

        for (size_t idx : m_Order)
        {
            RenderPass& pass = *m_Nodes[idx].pass;
            if (!pass.IsEnabled()) continue;

            pass.Prepare(ctx);
            pass.Execute(ctx);
            pass.Finalize(ctx);
        }
    }

    void RenderGraph::OnResize(uint32_t width, uint32_t height)
    {
        for (auto& n : m_Nodes) n.pass->OnResize(width, height);
    }

    void RenderGraph::Shutdown()
    {
        for (auto& n : m_Nodes) n.pass->Shutdown();
        m_Nodes.clear();
        m_Order.clear();
        m_Compiled = m_Initialized = false;
    }

    bool RenderGraph::HasPass(const std::string& name) const { return FindIndex(name) >= 0; }

    std::shared_ptr<RenderPass> RenderGraph::GetPass(const std::string& name) const
    {
        const int i = FindIndex(name);
        return i >= 0 ? m_Nodes[i].pass : nullptr;
    }

    std::string RenderGraph::GetDebugInfo() const
    {
        std::ostringstream oss;
        oss << "RenderGraph [" << (m_Compiled ? "compiled" : "NOT compiled") << "]\n";
        int order = 0;
        for (size_t idx : m_Order)
        {
            const auto& n = m_Nodes[idx];
            oss << "  [" << order++ << "] " << n.name << (n.pass->IsEnabled() ? "" : " (disabled)") << "\n";
        }
        return oss.str();
    }
}
