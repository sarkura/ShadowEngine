#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/RHI/Public/RHIEnums.h"

#include <string>

namespace ShadowEngine
{
    class RHITexture;

    class RHISwapChain : public NonCopyable
    {
        public:
            virtual ~RHISwapChain() = default;

            [[nodiscard]] virtual RHITexture& GetCurrentBackBuffer() = 0;
            [[nodiscard]] virtual uint32 GetWidth() const = 0;
            [[nodiscard]] virtual uint32 GetHeight() const = 0;
            [[nodiscard]] virtual ERHIFormat GetFormat() const = 0;

            virtual bool Present() = 0;

            // The GPU must be idle and no back buffer may be referenced by pending work.
            virtual bool Resize(uint32 Width, uint32 Height, std::string* ErrorMessage = nullptr) = 0;
    };
}
