#pragma once
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>

namespace DXEngine::Rendering
{
	class RenderPass;
	class FrameContext;

	/// <summary>
	/// RenderGraph - manages render passes and their dependencies
	/// 
	/// Modern frame Graph approach - declares what to render and in what order,
	/// with automatic resource management and dependency resolution
	/// </summary>
	class RenderGraph
	{
	public:
		struct PassNode
		{
			std::string name;
			std::shared_ptr<RenderPass> pass;
			std::vector<std::string> dependencies;  // Must execute after these
			std::vector<std::string> outputs;  // produce these resources
			bool enabled = true;
			int executionOrder = 0;  // Computed during compilation
		};

	public:
		RenderGraph() = default;
		~RenderGraph() = default;

		/*
		*  Add a render pass the graph
		*  Example:
		*	graph.AddPass<ShadowPass>("shadows");
		*	graph.AddPass<gbufferPass>("gbuffer").DependsOn("shadows");
		*/

		template<typename T, typename... Args>
		RenderGraph&& AddPass(const std::string& name, Args&&... args)
		{
			auto pass = std::make_shared<T>(std::forward<Args>(args)...);
			return AddPass(name, pass);
		}

		RenderGraph& AddPass(const std::string& name, std::shared_ptr<RenderPass> pass);

		//Declare dependencies between passes
		RenderGraph& DependsOn(const std::string& passName, const std::string& dependencyName);
		
		//Declare output resources
		RenderGraph& Produces(const std::string& passName, const std::string& resourceName);

		//removes Pass
		void RemovePass(const std::string& name);

		//Enable/Disable pass without removing it
		void EnablePass(const std::string& name, bool enable = true);

		///Compile the Graph - resolves dependecies, determins execution order
		//must be called after adding passes and before execution
		bool Compile();

		//validate the graph for cycles and missing Dependencies
		bool Validate(std::string& errorMessage)const;

		//Execute all passes in the correct order
		void Execute(FrameContext& context);

		//Execute a specific pass and its dependencies
		void ExecutePass(const std::string& name, FrameContext& context);

		///////Queries
		bool HasPass(const std::string& name)const;
		std::shared_ptr<RenderPass> GetPass(const std::string& name)const;
		const std::vector<PassNode>& GetExecutionOrder() const { return m_ExecutionOrder; }

		/////Utilities
		void Clear();
		size_t GetPassCount()const { return m_Passes.size(); }
		std::string GetDebugInfo()const;

	private:
		//Topological sort for Dependecy resolution
		bool TopologicalSort();
		void VistNode(const std::string& name, std::unordered_map<std::string, int>& visited, std::vector<std::string>& sorted);


	private:
		std::unordered_map<std::string, PassNode> m_Passes;
		std::vector<PassNode> m_ExecutionOrder;
		bool m_IsCompiled = false;

	};

}

