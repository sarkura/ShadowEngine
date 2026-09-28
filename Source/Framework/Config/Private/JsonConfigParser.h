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
                ViewportSetting& Setting,
                std::string* ErrorMessage = nullptr);

            static bool ValidateViewportSetting(
                const ViewportSetting& Setting,
                std::string* ErrorMessage = nullptr);
    };
}
