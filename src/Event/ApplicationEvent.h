#pragma once

#include "Event.h"
#include <sstream>

namespace DXEngine {

	class  WindowResizeEvent : public Event
	{
	public:
		WindowResizeEvent(unsigned int width, unsigned int height)
			: m_width(width), m_height(height)
		{
		}
		inline unsigned int GetWidth() const { return m_width; }
		inline unsigned int GetHeight() const { return m_height; }

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "WindowResizeEvent: " << m_width << ", " << m_height;
			return ss.str();
		}
		EVENT_CLASS_TYPE(WindowResize)
			EVENT_CLASS_CATEGORY(EventCategoryApplication)


	private:
		unsigned int m_width = NULL, m_height = NULL;
	};

	class  WindowCloseEvent : public Event
	{
	public:
		WindowCloseEvent() = default;
		std::string ToString() const override
		{
			return "WindowCloseEvent";
		}
		EVENT_CLASS_TYPE(WindowClose)
			EVENT_CLASS_CATEGORY(EventCategoryApplication)
	};

	class  WindowFocusEvent : public Event
	{
	public:
		WindowFocusEvent() {}
		std::string ToString() const override
		{
			return "WindowFocusEvent";
		}

		EVENT_CLASS_TYPE(WindowFocus)
			EVENT_CLASS_CATEGORY(EventCategoryApplication)
	};
	class  WindowLostFocusEvent : public Event
	{
	public:
		WindowLostFocusEvent() = default;

		std::string ToString() const override
		{
			return "WindowLostFocusEvent";
		}
		EVENT_CLASS_TYPE(WindowLostFocus)
			EVENT_CLASS_CATEGORY(EventCategoryApplication)
	};
	class  AppTickEvent : public Event
	{
	public:
		AppTickEvent() {}
		std::string ToString() const override
		{
			return "AppTickEvent";
		}
		EVENT_CLASS_TYPE(AppTick)
			EVENT_CLASS_CATEGORY(EventCategoryApplication)
	};

	class AppUpdateEvent : public Event
	{
	public:
		AppUpdateEvent() {}
		std::string ToString() const override
		{
			return "AppUpdateEvent";
		}
		EVENT_CLASS_TYPE(AppUpdate)
			EVENT_CLASS_CATEGORY(EventCategoryApplication)

	};

	class AppRenderEvent : public Event
	{
	public:
		AppRenderEvent() {}
		std::string ToString() const override
		{
			return "AppRenderEvent";
		}
		EVENT_CLASS_TYPE(AppRender)
			EVENT_CLASS_CATEGORY(EventCategoryApplication)
	};
}