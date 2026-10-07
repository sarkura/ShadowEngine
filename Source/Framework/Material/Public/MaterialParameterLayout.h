#pragma once

#include "Framework/Common/Public/Types.h"

#include <string>
#include <vector>

namespace ShadowEngine
{
    enum class EMaterialParameterType : uint8
    {
        Float4,
        Texture2D,
        Sampler,
    };

    struct MaterialParameterLayout
    {
        struct Entry
        {
            std::string Name;
            EMaterialParameterType Type = EMaterialParameterType::Float4;
        };

        std::vector<Entry> Entries;

        [[nodiscard]] bool Has(const std::string& Name) const;
        [[nodiscard]] bool HasTexture(const std::string& Name) const;
    };
}
