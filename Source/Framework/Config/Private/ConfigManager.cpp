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
        EngineSetting::ViewportSetting LoadedViewport;
        if (!JsonConfigParser::LoadViewportSetting(
                ConfigDirectory / "Engine.json",
                LoadedViewport,
                ErrorMessage))
        {
            return false;
        }

        RenderSetting::RHISetting LoadedRHI;
        if (!JsonConfigParser::LoadRHISetting(
                ConfigDirectory / "Renderer.json",
                LoadedRHI,
                ErrorMessage))
        {
            return false;
        }

        SceneSetting::Setting LoadedScene;
        if (!JsonConfigParser::LoadSceneSetting(
                ConfigDirectory / "Scene.json",
                LoadedScene,
                ErrorMessage))
        {
            return false;
        }

        const std::scoped_lock Lock(ConfigMutex);
        ConfigRoot = ConfigDirectory;
        Viewport = std::move(LoadedViewport);
        RHI = std::move(LoadedRHI);
        Scene = std::move(LoadedScene);
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

    EngineSetting::ViewportSetting ConfigManager::GetViewportSetting() const
    {
        const std::scoped_lock Lock(ConfigMutex);
        return Viewport;
    }

    RenderSetting::RHISetting ConfigManager::GetRHISetting() const
    {
        const std::scoped_lock Lock(ConfigMutex);
        return RHI;
    }

    SceneSetting::Setting ConfigManager::GetSceneSetting() const
    {
        const std::scoped_lock Lock(ConfigMutex);
        return Scene;
    }

    bool ConfigManager::SetViewportSetting(
        EngineSetting::ViewportSetting Setting,
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
