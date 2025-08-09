#include "dxpch.h"
#include "UIElement.h"

namespace DXEngine
{
	UIElement::UIElement(const UIRect& bounds)
		: m_Bounds(bounds)
	{
	}

	void UIElement::AddChild(std::shared_ptr<UIElement> child)
	{
		if (child && child.get() != this)
		{
			child->m_parent = this;
			m_Children.push_back(child);
		}
	}

	void UIElement::RemoveChild(std::shared_ptr<UIElement> child)
	{
		auto it = std::find(m_Children.begin(), m_Children.end(), child);
		if (it != m_Children.end())
		{
			(*it)->m_parent = nullptr;
			m_Children.erase(it);
		}
	}
}