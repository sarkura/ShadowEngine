#include "Framework/Common/Public/BaseApplication.h"

#include "Framework/Common/Public/Log.h"
#include "Framework/Config/Public/ConfigManager.h"

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
                Log::Error("Config initialization failed: {}", ErrorMessage);
                return -1;
            }
        }

        bQuit = false;
        return 0;
    }

    void BaseApplication::Finalize()
    {
        EngineInstance.Finalize();
    }
    
    void BaseApplication::Tick(float DeltaTime)
    {
        EngineInstance.Tick(DeltaTime);
    }

    bool BaseApplication::IsQuit()
    {       
        return bQuit;
    }
}       
