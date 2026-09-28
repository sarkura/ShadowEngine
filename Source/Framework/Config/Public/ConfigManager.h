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
            [[nodiscard]] ViewportSetting GetViewportSetting() const;

            bool SetViewportSetting(
                ViewportSetting Setting,
                std::string* ErrorMessage = nullptr);

            ConfigManager(const ConfigManager&) = delete;
            ConfigManager& operator=(const ConfigManager&) = delete;
            ConfigManager(ConfigManager&&) = delete;
            ConfigManager& operator=(ConfigManager&&) = delete;

        private:
            ConfigManager() = default;

            mutable std::mutex ConfigMutex;
            std::filesystem::path ConfigRoot = "Config";
            ViewportSetting Viewport;
            bool bInitialized = false;
    };
}
