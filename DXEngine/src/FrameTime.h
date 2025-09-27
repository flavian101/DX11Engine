#pragma once
#include <chrono>

namespace DXEngine {

	class FrameTime
	{
	public:
		FrameTime() noexcept;
		float Mark() noexcept;
		float Peek() const noexcept;
		float Duration()const noexcept;
		void Reset() noexcept;
		operator float() { return Mark(); }
	private:
		std::chrono::steady_clock::time_point m_Last;
		std::chrono::steady_clock::time_point m_Start;
	};

}