#include "dxpch.h"
#include "FrameTime.h"
namespace DXEngine {

	using namespace std::chrono;
	FrameTime::FrameTime() noexcept
	{
		m_Last = steady_clock::now();
		m_Start = m_Last;
	}

	float FrameTime::Mark()  noexcept
	{
		const auto old = m_Last;
		m_Last = steady_clock::now();
		const duration<float> frameTime = m_Last - old;
		return frameTime.count();
	}

	float FrameTime::Peek() const noexcept
	{
		return duration<float>(steady_clock::now() - m_Last).count();
	}
	float FrameTime::Duration() const noexcept
	{
		return duration<float>(steady_clock::now() - m_Start).count();
	}

	void FrameTime::Reset() noexcept
	{
		m_Start = steady_clock::now();
		m_Last = m_Start;
	}
}