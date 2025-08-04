#include "FrameTime.h"
namespace DXEngine {

	using namespace std::chrono;
	FrameTime::FrameTime() noexcept
	{
		last = steady_clock::now();
	}

	float FrameTime::Mark() noexcept
	{
		const auto old = last;
		last = steady_clock::now();
		const duration<float> frameTime = last - old;
		return frameTime.count();
	}

	float FrameTime::Peek() const noexcept
	{
		return duration<float>(steady_clock::now() - last).count();
	}
}