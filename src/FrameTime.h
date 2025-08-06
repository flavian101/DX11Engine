#pragma once
#include <chrono>

namespace DXEngine {

	class FrameTime
	{
	public:
		FrameTime() noexcept;
		float Mark() noexcept;
		float Peek() const noexcept;
		operator float() { return Mark(); }
	private:
		std::chrono::steady_clock::time_point last;
	};

}