#include "dxpch.h"
#include "RenderGraph.h"
#include "RenderPass.h"
#include "FrameContext.h"

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
	}
	void RenderGraph::ExecutePass(const std::string& name, FrameContext& context)
	{
	}
	bool RenderGraph::HasPass(const std::string& name) const
	{
		return false;
	}
	std::shared_ptr<RenderPass> RenderGraph::GetPass(const std::string& name) const
	{
		return std::shared_ptr<RenderPass>();
	}
	void RenderGraph::Clear()
	{
	}
	std::string RenderGraph::GetDebugInfo() const
	{
		return std::string();
	}
	bool RenderGraph::TopologicalSort()
	{
		//kahn's algorithim: build in-degree map, process nodes with zero in-degree

		return false;
	}
	void RenderGraph::VistNode(const std::string& name, std::unordered_map<std::string, int>& visited, std::vector<std::string>& sorted)
	{
	}
}