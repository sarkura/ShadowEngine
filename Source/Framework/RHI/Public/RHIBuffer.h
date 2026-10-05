#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Common/Public/Types.h"

#include <span>

namespace ShadowEngine
{
    class RHIBuffer : public NonCopyable
    {
        public:
            virtual ~RHIBuffer() = default;

            [[nodiscard]] virtual uint32 GetSize() const = 0;
            [[nodiscard]] virtual uint32 GetStride() const = 0;
            virtual bool Update(uint32 Offset, std::span<const uint8> Data) = 0;
    };
}
