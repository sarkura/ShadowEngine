#pragma once

#include "ThirdPartyAdapter/Serialization/Public/RapidJSONAdapter.h"

#include <filesystem>
#include <string>


namespace ShadowEngine
{
    bool ParseJsonObject(
        const std::filesystem::path& FilePath,
        const char* Kind,
        RapidJSONDocument& Document,
        std::string* ErrorMessage);

    bool RequireMembers(
        const RapidJSONValue& Object,
        const std::filesystem::path& FilePath,
        const char* Kind,
        const char* const* Names,
        size_t NameCount,
        std::string* ErrorMessage);
}
