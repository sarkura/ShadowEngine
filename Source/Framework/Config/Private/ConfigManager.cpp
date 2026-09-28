#include "Framework/Config/Public/ConfigManager.h"

#include "Framework/Config/Private/JsonConfigParser.h"

#include <utility>

namespace ShadowEngine
{
    ConfigManager& ConfigManager::Get()
    {
        static ConfigManager Instance;
        return Instance;
    }

    bool ConfigManager::Initialize(
        const std::filesystem::path& ConfigDirectory,
        std::string* ErrorMessage)
    {
        ViewportSetting LoadedViewport;
        if (!JsonConfigParser::LoadViewportSetting(
                ConfigDirectory / "ViewportSetting.json",
                LoadedViewport,
                ErrorMessage))
        {
            return false;
        }

        const std::scoped_lock Lock(ConfigMutex);
        ConfigRoot = ConfigDirectory;
        Viewport = std::move(LoadedViewport);
        bInitialized = true;
        return true;
    }

    bool ConfigManager::Reload(std::string* ErrorMessage)
    {
        std::filesystem::path CurrentConfigRoot;
        {
            const std::scoped_lock Lock(ConfigMutex);
            CurrentConfigRoot = ConfigRoot;
        }

        return Initialize(CurrentConfigRoot, ErrorMessage);
    }

    bool ConfigManager::IsInitialized() const
    {
        const std::scoped_lock Lock(ConfigMutex);
        return bInitialized;
    }

    ViewportSetting ConfigManager::GetViewportSetting() const
    {
        const std::scoped_lock Lock(ConfigMutex);
        return Viewport;
    }

    bool ConfigManager::SetViewportSetting(
        ViewportSetting Setting,
        std::string* ErrorMessage)
    {
        if (!JsonConfigParser::ValidateViewportSetting(
                Setting,
                ErrorMessage))
        {
            return false;
        }

        const std::scoped_lock Lock(ConfigMutex);
        Viewport = std::move(Setting);
        return true;
    }
}
