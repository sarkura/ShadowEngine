#pragma once

#include "Framework/Config/Public/ConfigSetting.h"

#include <filesystem>
#include <mutex>
#include <string>

namespace ShadowEngine
{
    class ConfigManager final
    {
        public:
            static ConfigManager& Get();

            bool Initialize(
                const std::filesystem::path& ConfigDirectory = "Config",
                std::string* ErrorMessage = nullptr);

            bool Reload(std::string* ErrorMessage = nullptr);

            [[nodiscard]] bool IsInitialized() const;
            [[nodiscard]] EngineSetting::ViewportSetting GetViewportSetting() const;
            [[nodiscard]] RenderSetting::RHISetting GetRHISetting() const;
            [[nodiscard]] SceneSetting::Setting GetSceneSetting() const;

            bool SetViewportSetting(
                EngineSetting::ViewportSetting Setting,
                std::string* ErrorMessage = nullptr);

            ConfigManager(const ConfigManager&) = delete;
            ConfigManager& operator=(const ConfigManager&) = delete;
            ConfigManager(ConfigManager&&) = delete;
            ConfigManager& operator=(ConfigManager&&) = delete;

        private:
            ConfigManager() = default;

            mutable std::mutex ConfigMutex;
            std::filesystem::path ConfigRoot = "Config";
            EngineSetting::ViewportSetting Viewport;
            RenderSetting::RHISetting RHI;
            SceneSetting::Setting Scene;
            bool bInitialized = false;
    };
}
