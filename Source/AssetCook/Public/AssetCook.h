#pragma once

#include "CookedAsset/Public/CookedAsset.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ShadowEngine
{
    struct CookMeshSlot
    {
        uint32 Slot = 0;
        bool bRequireTexCoord = false;
    };

    [[nodiscard]] bool CookMesh(
        const std::filesystem::path& Source,
        const std::vector<CookMeshSlot>& Slots,
        const std::filesystem::path& Output,
        std::string* ErrorMessage);

    [[nodiscard]] bool CookTexture(
        const std::filesystem::path& Source,
        ECookedTextureEncoding Encoding,
        const std::filesystem::path& Output,
        std::string* ErrorMessage);

    [[nodiscard]] bool CookEnvironment(
        const std::filesystem::path& Source,
        const std::filesystem::path& Output,
        std::string* ErrorMessage);
}
