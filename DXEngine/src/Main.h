#pragma once
#include "DXEngine.h"

extern DXEngine::Application* DXEngine::CreateApplication();

int main(int argc, char** argv)
{
    auto application = DXEngine::CreateApplication();
    int result = application->createLoop();
    delete application;
    return result;
}
