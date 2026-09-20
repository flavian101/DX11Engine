#include "dxpch.h"
#include "RenderGraph.h"
#include "RenderPass.h"
#include "FrameContext.h"

//#include "passes/ClearPass.h"
//#include "passes/ShadowPass.h"
//#include "passes/ForwardOpaquePass.h"
//#include "passes/ForwardTransparentPass.h"
//#include "passes/DepthPrepassPass.h"
//#include "passes/GBufferPass.h"
//#include "passes/DeferredLightingPass.h"
//#include "passes/LightCullingPass.h"
//#include "passes/ForwardPlusOpaquePass.h"
//#include "passes/UIPass.h"

#include <sstream>
#include <stdexcept>

namespace DXEngine::Rendering{

	RenderGraph& RenderGraph::AddPass(const std::string& name, std::shared_ptr<RenderPass> pass)
	{
		if (m_Passes.count(name))
		{
			OutputDebugStringA(("WARNING: RenderGraph::AddPass - pass already exists: " + name + "\n").c_str());
			return *this;
		}
		PassNode node;
		node.name = name;
		node.pass = pass;
		m_Passes[name] = std::move(node);
		m_IsCompiled = false;

		return *this;
	}
	RenderGraph& RenderGraph::DependsOn(const std::string& passName, const std::string& dependencyName)
	{
		auto it = m_Passes.find(passName);
		if (it == m_Passes.end())
		{
			OutputDebugStringA(("WARNING: RenderGraph::DependsOn - pass not found: " + passName + "\n").c_str());
			return *this;
		}
		it->second.dependencies.push_back(dependencyName);
		m_IsCompiled = false;
		return *this;
	}
	RenderGraph& RenderGraph::Produces(const std::string& passName, const std::string& resourceName)
	{
		auto it = m_Passes.find(passName);
		if (it == m_Passes.end())
		{
			OutputDebugStringA(("WARNING: RenderGraph::Produces - pass not found: " + passName + "\n").c_str());
			return *this;
		}
		it->second.outputs.push_back(resourceName);
		return *this;
	}
	void RenderGraph::RemovePass(const std::string& name)
	{
		m_Passes.erase(name);
		m_IsCompiled = false;
	}
	void RenderGraph::EnablePass(const std::string& name, bool enable)
	{
		auto it = m_Passes.find(name);
		if (it != m_Passes.end())
		{
			it->second.enabled = enable;
		}
	}
	bool RenderGraph::Compile()
	{
		std::string error;
		if (!Validate(error))
		{
			OutputDebugStringA(("ERROR: RenderGraph::Compile - " + error + "\n").c_str());
			return false;
		}
		if (!TopologicalSort())
		{
			OutputDebugStringA("ERROR: RenderGraph::Compile - cycle detected in pass dependencies\n");
			return false;
		}

		m_IsCompiled = true;
		OutputDebugStringA(("RenderGraph compiled: " + std::to_string(m_ExecutionOrder.size()) + " passes\n").c_str());

		return true;
	}
	bool RenderGraph::Validate(std::string& errorMessage) const
	{
		//check all declared Dependencies actually exist as passes
		for (const auto& [name, node] : m_Passes) {
			for (const auto& dep : node.dependencies)
			{
				if (!m_Passes.count(dep))
				{
					errorMessage = "Pass '" + name + "' depends on unknown pass '" + dep + "'";
					return false;
				}
			}
		}
		return true;
	}
	void RenderGraph::Execute(FrameContext& context)
	{
		if (!m_IsCompiled)
		{
			OutputDebugStringA("Error: RenderGraph::Execute - graph not compiled\n");
			return;
		}

		for (const auto& node  : m_ExecutionOrder)
		{
			if (!node.enabled || !node.pass)
			{
				continue;
			}

			node.pass->Prepare(context);
			node.pass->Execute(context);
			node.pass->Finalize(context);

		}
	}
	void RenderGraph::ExecutePass(const std::string& name, FrameContext& context)
	{
		auto it = m_Passes.find(name);
		if (it == m_Passes.end() || !it->second.pass)
		{
			return;
		}

		//Execute dependencies first (recursive)
		for (const auto& dep : it->second.dependencies)
			ExecutePass(dep, context);

		if (!it->second.enabled)
		{
			return;
		}

		it->second.pass->Prepare(context);
		it->second.pass->Execute(context);
		it->second.pass->Finalize(context);
	}
	bool RenderGraph::HasPass(const std::string& name) const
	{
		return m_Passes.count(name) > 0;
	}
	std::shared_ptr<RenderPass> RenderGraph::GetPass(const std::string& name) const
	{
		auto it = m_Passes.find(name);
		return (it!= m_Passes.end())? it->second.pass : nullptr;
	}
	void RenderGraph::Clear()
	{
		m_Passes.clear();
		m_ExecutionOrder.clear();
		m_IsCompiled = false;
	}
	std::string RenderGraph::GetDebugInfo() const
	{
		std::ostringstream oss;
		oss << "RenderGraph [" << (m_IsCompiled ? "compiled" : "NOT compiled") << "]\n";
		oss << "  Passes (" << m_Passes.size() << "):\n";
		for (const auto& node : m_ExecutionOrder)
		{
			oss << "    [" << node.executionOrder << "] "
				<< node.name
				<< (node.enabled ? "" : " (disabled)");
			if (!node.dependencies.empty())
			{
				oss << " <- ";
				for (size_t i = 0; i < node.dependencies.size(); ++i)
				{
					if (i) oss << ", ";
					oss << node.dependencies[i];
				}
			}
			oss << "\n";
		}
		return oss.str();
	}
	bool RenderGraph::TopologicalSort()
	{
		//kahn's algorithm: build in-degree map, process nodes with zero in-degree
		std::unordered_map<std::string, int> inDegree;
		for (const auto& [name, node] : m_Passes)
		{
			inDegree[name] = 0;
		}

		for (const auto& [name, node] : m_Passes)
			for (const auto& dep : node.dependencies)
				inDegree[name]++;// this pass has one more prerequisite

		//start with passes that have no dependencies
		std::vector<std::string> queue;
		for (const auto& [name, deg] : inDegree)
			if (deg == 0)
				queue.push_back(name);

		m_ExecutionOrder.clear();
		int order = 0;
		while (!queue.empty())
		{
			std::string current = queue.back();
			queue.pop_back();

			auto& node = m_Passes.at(current);
			node.executionOrder = order++;
			m_ExecutionOrder.push_back(node);

			//reduce in-degree for passes that depend on current
			for (auto& [name, node2] : m_Passes)
			{
				auto& deps = node2.dependencies;
				if (std::find(deps.begin(), deps.end(), current) != deps.end())
				{
					inDegree[name]--;
					if (inDegree[name] == 0)
					{
						queue.push_back(name);
					}
				}
			}

		}
		//if not all passes are in order, there's a cycle
		return m_ExecutionOrder.size()== m_Passes.size();
	}
	void RenderGraph::VisitNode(const std::string& name, std::unordered_map<std::string, int>& visited, std::vector<std::string>& sorted)
	{
	}
	//namespace RenderGraphPresets
	//{
	//	// ---- Forward: clear → shadow → opaque → transparent → ui ---------------
	//	std::shared_ptr<RenderGraph> CreateForwardGraph()
	//	{
	//		auto g = std::make_shared<RenderGraph>();
	//		g->AddPass<ClearPass>("clear");
	//		g->AddPass<ShadowPass>("shadow");
	//		g->AddPass<ForwardOpaquePass>("opaque");
	//		g->AddPass<ForwardTransparentPass>("transparent");
	//		g->AddPass<UIPass>("ui");
	//		g->DependsOn("shadow", "clear");
	//		g->DependsOn("opaque", "shadow");
	//		g->DependsOn("transparent", "opaque");
	//		g->DependsOn("ui", "transparent");
	//		return g->Compile() ? g : nullptr;
	//	}

	//	// ---- Deferred: clear → shadow → depth → gbuffer → lighting → transparent → ui
	//	std::shared_ptr<RenderGraph> CreateDeferredGraph()
	//	{
	//		auto g = std::make_shared<RenderGraph>();
	//		g->AddPass<ClearPass>("clear");
	//		g->AddPass<ShadowPass>("shadow");
	//		g->AddPass<DepthPrepassPass>("depth_prepass");
	//		g->AddPass<GBufferPass>("gbuffer");
	//		g->AddPass<DeferredLightingPass>("lighting");
	//		g->AddPass<ForwardTransparentPass>("transparent");
	//		g->AddPass<UIPass>("ui");
	//		g->DependsOn("shadow", "clear");
	//		g->DependsOn("depth_prepass", "shadow");
	//		g->DependsOn("gbuffer", "depth_prepass");
	//		g->DependsOn("lighting", "gbuffer");
	//		g->DependsOn("transparent", "lighting");
	//		g->DependsOn("ui", "transparent");
	//		return g->Compile() ? g : nullptr;
	//	}

	//	// ---- Forward+: clear → shadow → depth → light_culling → fp_opaque → transparent → ui
	//	std::shared_ptr<RenderGraph> CreateForwardPlusGraph()
	//	{
	//		auto g = std::make_shared<RenderGraph>();
	//		g->AddPass<ClearPass>("clear");
	//		g->AddPass<ShadowPass>("shadow");
	//		g->AddPass<DepthPrepassPass>("depth_prepass");
	//		g->AddPass<LightCullingPass>("light_culling");
	//		g->AddPass<ForwardPlusOpaquePass>("fp_opaque");
	//		g->AddPass<ForwardTransparentPass>("transparent");
	//		g->AddPass<UIPass>("ui");
	//		g->DependsOn("shadow", "clear");
	//		g->DependsOn("depth_prepass", "shadow");
	//		g->DependsOn("light_culling", "depth_prepass");
	//		g->DependsOn("fp_opaque", "light_culling");
	//		g->DependsOn("transparent", "fp_opaque");
	//		g->DependsOn("ui", "transparent");
	//		return g->Compile() ? g : nullptr;
	//	}

	//	// ---- Minimal: just clear + forward opaque (for unit tests) ---------------
	//	std::shared_ptr<RenderGraph> CreateMinimalGraph()
	//	{
	//		auto g = std::make_shared<RenderGraph>();
	//		g->AddPass<ClearPass>("clear");
	//		g->AddPass<ForwardOpaquePass>("opaque");
	//		g->DependsOn("opaque", "clear");
	//		return g->Compile() ? g : nullptr;
	//	}
	//}

}