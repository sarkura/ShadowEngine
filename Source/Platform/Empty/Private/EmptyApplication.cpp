#include "Platform/Empty/Public/EmptyApplication.h"

namespace ShadowEngineInterface
{
    EmptyApplication::EmptyApplication() = default;

    EmptyApplication::~EmptyApplication() = default;

    EmptyApplication* NewEmptyApplication = new EmptyApplication();
    IApplication* G_App = NewEmptyApplication;
}