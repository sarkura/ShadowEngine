#include "Platform/Empty/Public/EmptyApplication.h"

#include <iostream>

namespace ShadowEngine
{
    EmptyApplication::EmptyApplication() = default;

    EmptyApplication::~EmptyApplication() = default;

    int EmptyApplication::Initialize()
    {
        std::cout << "Platform Unsupported" << std::endl;
        bQuit = true;
        return 0;
    }

    IApplication* G_App = new EmptyApplication();
}