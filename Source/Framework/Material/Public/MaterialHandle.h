#pragma once

#include "Framework/Common/Public/Types.h"

namespace ShadowEngine
{
    struct MaterialHandle
    {
        static constexpr uint32 Invalid = 0xFFFFFFFFU;

        uint32 Index = Invalid;

        [[nodiscard]] bool IsValid() const
        {
            return Index != Invalid;
        }
    };

    struct MaterialInstanceHandle
    {
        static constexpr uint32 Invalid = 0xFFFFFFFFU;

        uint32 Index = Invalid;

        [[nodiscard]] bool IsValid() const
        {
            return Index != Invalid;
        }
    };
}
