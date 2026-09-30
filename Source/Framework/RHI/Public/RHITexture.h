#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/RHI/Public/RHIEnums.h"

namespace ShadowEngine
{
    class RHITexture : public NonCopyable
    {
        public:
            virtual ~RHITexture() = default;

            [[nodiscard]] virtual uint32 GetWidth() const = 0;
            [[nodiscard]] virtual uint32 GetHeight() const = 0;
            [[nodiscard]] virtual ERHIFormat GetFormat() const = 0;
    };
}
