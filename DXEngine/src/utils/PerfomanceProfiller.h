#pragma once
#include <string>
#include <chrono>
#include <unordered_map>
namespace DXEngine {

    class PerformanceProfiler
    {
    public:
        static void BeginFrame();
        static void EndFrame();
        static void BeginSection(const std::string& name);
        static void EndSection(const std::string& name);

        static float GetFrameTime();
        static float GetSectionTime(const std::string& name);
        static std::string GetPerformanceReport();

    private:
        struct ProfileSection {
            std::chrono::high_resolution_clock::time_point start;
            float totalTime = 0.0f;
            uint32_t callCount = 0;
        };

        static std::unordered_map<std::string, ProfileSection> s_Sections;
        static std::chrono::high_resolution_clock::time_point s_FrameStart;
        static float s_FrameTime;
    };

    // Macro for easy profiling
#define PROFILE_SCOPE(name) \
        DXEngine::PerformanceProfiler::BeginSection(name); \
        auto __profile_guard = [](){ DXEngine::PerformanceProfiler::EndSection(name); }; \
        std::unique_ptr<decltype(__profile_guard), decltype(__profile_guard)> __profile_raii(&__profile_guard, __profile_guard)

}