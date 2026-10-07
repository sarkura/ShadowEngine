#pragma once

#include "Framework/Common/Public/Types.h"

namespace ShadowEngine
{
    struct MeshHandle
    {
        static constexpr uint32 Invalid = 0xFFFFFFFFU;

        uint32 Index = Invalid;

        [[nodiscard]] bool IsValid() const
        {
            return Index != Invalid;
        }
    };

    struct TextureAssetHandle
    {
        static constexpr uint32 Invalid = 0xFFFFFFFFU;

        uint32 Index = Invalid;

        [[nodiscard]] bool IsValid() const
        {
            return Index != Invalid;
        }
    };

    struct SamplerHandle
    {
        static constexpr uint32 Invalid = 0xFFFFFFFFU;

        uint32 Index = Invalid;

        [[nodiscard]] bool IsValid() const
        {
            return Index != Invalid;
        }
    };
}
