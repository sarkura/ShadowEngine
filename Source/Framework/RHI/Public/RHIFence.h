#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Common/Public/Types.h"

namespace ShadowEngine
{
    class RHIFence : public NonCopyable
    {
        public:
            virtual ~RHIFence() = default;

            [[nodiscard]] virtual uint64 GetCompletedValue() const = 0;
            virtual void Wait(uint64 Value) = 0;
    };
}
