#pragma once

#include "Framework/Config/Public/ConfigSetting.h"

#include <filesystem>
#include <string>

namespace ShadowEngine
{
    class JsonConfigParser final
    {
        public:
            JsonConfigParser() = delete;

            static bool LoadViewportSetting(
                const std::filesystem::path& FilePath,
                EngineSetting::ViewportSetting& Setting,
                std::string* ErrorMessage = nullptr);

            static bool ValidateViewportSetting(
                const EngineSetting::ViewportSetting& Setting,
                std::string* ErrorMessage = nullptr);

            static bool LoadRHISetting(
                const std::filesystem::path& FilePath,
                RenderSetting::RHISetting& Setting,
                std::string* ErrorMessage = nullptr);

            static bool ValidateRHISetting(
                const RenderSetting::RHISetting& Setting,
                std::string* ErrorMessage = nullptr);
    };
}
