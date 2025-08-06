#pragma once
#include "Event/Event.h"
#include "FrameTime.h"

namespace DXEngine {
	class Layer
	{
	public:
		Layer(const std::string& name = "layer");
		virtual ~Layer();

		virtual void OnAttach(){}
		virtual void OnDetach(){}
		virtual void OnUpdate(FrameTime dt){}
		virtual void OnUIRender(){}
		virtual void OnEvent(Event& event){}
	protected:
		std::string m_DebugName;
	};
}

