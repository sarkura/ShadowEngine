#pragma once

#include <filesystem>
#include <string>

#include <rapidjson/document.h>

namespace ShadowEngine
{
    bool ParseJsonObject(
        const std::filesystem::path& FilePath,
        const char* Kind,
        rapidjson::Document& Document,
        std::string* ErrorMessage);

    bool RequireMembers(
        const rapidjson::Value& Object,
        const std::filesystem::path& FilePath,
        const char* Kind,
        const char* const* Names,
        size_t NameCount,
        std::string* ErrorMessage);
}
