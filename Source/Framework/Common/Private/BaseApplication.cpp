#include "Framework/Common/Public/BaseApplication.h"

namespace ShadowEngineInterface
{
    BaseApplication::BaseApplication()
    {
    }

    BaseApplication::~BaseApplication()
    {
    }

    int BaseApplication::Initialize()
    {       
        bQuit = false;
        return 0;
    }

    void BaseApplication::Finalize()
    {
    }
    
    void BaseApplication::Tick(float DeltaTime)
    {
    }

    bool BaseApplication::IsQuit()
    {       
        return bQuit;
    }
}       