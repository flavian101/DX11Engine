#pragma once
#include "UIElement.h"

namespace DXEngine
{
	enum class TextAlignment
	{
		Left,
		Center,
		Right
	};

	class UIText : UIElement
	{
	public:
        UIText(const std::string& text, const UIRect& bounds);
        virtual ~UIText() = default;

        // UIElement interface
        void Update(FrameTime dt) override;
        bool HandleInput(float mouseX, float mouseY, bool leftClick, bool rightClick) override;

        // Text specific
        void SetText(const std::string& text) { m_Text = text; }
        const std::string& GetText() const { return m_Text; }

        void SetColor(const UIColor& color) { m_Color = color; }
        const UIColor& GetColor() const { return m_Color; }

        void SetFontSize(float size) { m_FontSize = size; }
        float GetFontSize() const { return m_FontSize; }

        void SetAlignment(TextAlignment alignment) { m_Alignment = alignment; }
        TextAlignment GetAlignment() const { return m_Alignment; }

        void SetWordWrap(bool wrap) { m_WordWrap = wrap; }
        bool GetWordWrap() const { return m_WordWrap; }

    private:
        std::string m_Text;
        UIColor m_Color = UIColor(1.0f, 1.0f, 1.0f, 1.0f);
        float m_FontSize = 16.0f;
        TextAlignment m_Alignment = TextAlignment::Left;
        bool m_WordWrap = false;
	};
}

