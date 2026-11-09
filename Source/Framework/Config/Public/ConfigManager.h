#pragma once

#include "Framework/Config/Public/ConfigSetting.h"
#include "Framework/Config/Public/UIConfigManager.h"
#include "Framework/Config/Public/UIConfigSetting.h"

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
            [[nodiscard]] EngineSetting::MovementSetting GetMovementSetting() const;
            [[nodiscard]] RenderSetting::RHISetting GetRHISetting() const;
            [[nodiscard]] SceneSetting::Setting GetSceneSetting() const;
            [[nodiscard]] GlobalTextureSetting::Setting GetGlobalTextureSetting() const;

            bool SetViewportSetting(
                EngineSetting::ViewportSetting Setting,
                std::string* ErrorMessage = nullptr);

            void SubmitDeferredUISetting(UIConfigSetting::DeferredUIParameter Setting);
            void SubmitForwardUISetting(UIConfigSetting::ForwardUIParameter Setting);
            void SubmitBlinnPhongUISetting(UIConfigSetting::BlinnPhongUIParameter Setting);
            void SubmitDebugUISetting(UIConfigSetting::DebugUIParameter Setting);
            [[nodiscard]] UIConfigSetting::DeferredUIParameter GetDeferredUISetting() const;
            [[nodiscard]] UIConfigSetting::ForwardUIParameter GetForwardUISetting() const;
            [[nodiscard]] UIConfigSetting::BlinnPhongUIParameter GetBlinnPhongUISetting() const;
            [[nodiscard]] UIConfigSetting::DebugUIParameter GetDebugUISetting() const;

            ConfigManager(const ConfigManager&) = delete;
            ConfigManager& operator=(const ConfigManager&) = delete;
            ConfigManager(ConfigManager&&) = delete;
            ConfigManager& operator=(ConfigManager&&) = delete;

        private:
            ConfigManager() = default;

            mutable std::mutex ConfigMutex;
            std::filesystem::path ConfigRoot = "Config";
            EngineSetting::ViewportSetting Viewport;
            EngineSetting::MovementSetting Movement;
            RenderSetting::RHISetting RHI;
            SceneSetting::Setting Scene;
            GlobalTextureSetting::Setting GlobalTextures;
            UIConfigManager UserInterface;
            bool bInitialized = false;
    };
}
