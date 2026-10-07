#pragma once

#include "Framework/Asset/Public/Description.h"

#include <filesystem>
#include <string>

namespace ShadowEngine
{
    class DescriptionParser final
    {
        public:
            DescriptionParser() = delete;

            static bool LoadMeshDescription(
                const std::filesystem::path& FilePath,
                MeshDescription& Description,
                std::string* ErrorMessage = nullptr);

            static bool LoadMaterialDescription(
                const std::filesystem::path& FilePath,
                MaterialDescription& Description,
                std::string* ErrorMessage = nullptr);
    };
}
