#pragma once
#include "DXEngine.h"

extern DXEngine::Application* DXEngine::CreateApplication();

int main(int argc, char** argv)
{
    try
    {
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
