#include "Framework/Common/Public/BaseApplication.h"

#include "Framework/Config/Public/ConfigManager.h"

#include <iostream>

namespace ShadowEngine
{
    BaseApplication::BaseApplication()
    {
    }

    BaseApplication::~BaseApplication()
    {
    }

    int BaseApplication::Initialize()
    {
        ConfigManager& Config = ConfigManager::Get();
        if (!Config.IsInitialized())
        {
            std::string ErrorMessage;
            if (!Config.Initialize("Config", &ErrorMessage))
            {
                std::cerr << "Config initialization failed: "
                          << ErrorMessage << std::endl;
                return -1;
            }
        }

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