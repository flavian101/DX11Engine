#pragma once
#include <DirectXMath.h>
#include <string>
#include <memory>
#include <vector>
#include "renderer/RendererCommand.h"
#include <FrameTime.h>

namespace DXEngine
{
	struct UIRect
	{
		float x, y, width, height;

		UIRect(float x = 0, float y =0, float w = 0, float h = 0)
			:x(x), y(y), width(w), height(h){ }

		bool Contains(float px, float py)const
		{
			return px >= x && px <= x + width && py >= y && py <= y + height;
		}
	};

	struct UIColor
	{
		float r, g, b, a;

		UIColor(float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f)
			: r(r), g(g), b(b), a(a) {
		}
	};

	class UIElement
	{
	public:
		UIElement(const UIRect& bounds = UIRect());
		virtual ~UIElement() = default;

		virtual void Update(FrameTime dt){}
		virtual bool HandleInput(float mouseX, float mouseY, bool leftClick, bool rightClick) { return false; }


		void setBounds(const UIRect& bounds) { m_Bounds = bounds; }
		const UIRect& GetBounds()const { return m_Bounds; }

		void SetVisible(bool visible) { m_Visible = visible; }
		bool IsVisible() const { return m_Visible; }

		void SetEnabled(bool enabled) { m_Enabled = enabled; }
		bool IsEnabled() const { return m_Enabled; }

		//hierearchy
		void AddChild(std::shared_ptr<UIElement> child);
		void RemoveChild(std::shared_ptr<UIElement> child);
		const std::vector<std::shared_ptr<UIElement>>& GetChildren()const { return m_Children; }


	private:
		UIRect  m_Bounds;
		bool m_Visible = true;
		bool m_Enabled = true;
		std::vector<std::shared_ptr<UIElement>> m_Children;
		UIElement* m_parent = nullptr;



	};
}

