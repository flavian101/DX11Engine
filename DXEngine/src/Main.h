#pragma once
#include "DXEngine.h"
#include <filesystem>
#include <vector>

extern DXEngine::Application* DXEngine::CreateApplication();

// The application's files are found from the project root (the folder that has assets/shaders), not from wherever it was
// started: look from the executable's folder, then from the working directory, and make the root the working directory.
// Nothing found leaves the working directory alone (the shader loader then says what it cannot find).
static void EnterProjectRoot()
{
    std::vector<std::filesystem::path> starts;
    wchar_t exe[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, exe, MAX_PATH);
    if (length > 0 && length < MAX_PATH) starts.push_back(std::filesystem::path(exe).parent_path());
    starts.push_back(std::filesystem::current_path());
    DXEngine::ProjectRoot::Enter(starts);
}

int main(int argc, char** argv)
{
    try
    {
        EnterProjectRoot();
        auto application = DXEngine::CreateApplication();
        int result = application->createLoop();
        delete application;
        return result;
    }
    catch (const std::exception& e)
    {
        MessageBoxA(nullptr, e.what(), "DXEngine", MB_OK | MB_ICONERROR);
        return -1;
    }
}
