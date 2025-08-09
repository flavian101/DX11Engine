#pragma once
#include <utils/UI/UIElement.h>

namespace DXEngine
{

	class UIPanel : public UIElement
	{
	public:
		UIPanel(const UIRect& bounds, const UIColor& backroundColor = UIColor(0.2f, 0.2f, 0.2f, 0.8f));
		virtual ~UIPanel() = default;

		//element interface
		void Update(FrameTime dt) override;
		void Render() override;
		bool HandleInput(float mouseX, float mouseY, bool leftClick, bool rightClick);
	
		//panel specific
		void SetBorderColor(const UIColor& color) { m_BorderColor = color; }
		const UIColor& GetBorderColor() const { return m_BorderColor; }

		void SetBorderWidth(float width) { m_BorderWidth = width; }
		float GetBorderWidth() const { return m_BorderWidth; }

		void SetCornerRadius(float radius) { m_CornerRadius = radius; }
		float GetCornerRadius() const { return m_CornerRadius; }


		//layout
		void AutoSizeToFitChildren();
		void ArrangeChildrenVertically(float spacinf = 5.0f);
		void ArrangeChildrenHorizontally(float spacinf = 5.0f);



	private:
		UIColor m_BackgroundColor;
		UIColor m_BorderColor = UIColor(0.5f, 0.5f, 0.5f, 1.0f);
		float m_BorderWidth = 1.0f;
		float m_CornerRadius = 0.0f;
	};

}

