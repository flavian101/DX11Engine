#pragma once
#include "Layer.h"
#include <vector>


namespace DXEngine {

	class LayerStack
	{
	public:
		LayerStack();
		~LayerStack();

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* overlay);
		void PopLayer(Layer* layer);
		void PopOverlay(Layer* overlay);

		// Detaches (top of the stack first) and deletes every layer. Application calls this BEFORE the renderer shuts
		// down: a layer owns scenes and GPU resources that must not outlive the device.
		void Clear();


		std::vector<Layer*>::iterator begin() { return m_Layers.begin(); }
		std::vector<Layer*>::iterator end() { return m_Layers.end(); }

	private:
		std::vector<Layer*> m_Layers;
		unsigned int m_LayerInsertIndex = 0;
	};
}
