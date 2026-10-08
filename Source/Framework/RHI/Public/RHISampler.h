#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Common/Public/Types.h"

namespace ShadowEngine
{
    class RHISampler : public NonCopyable
    {
        public:
            virtual uint32 GetDescriptorIndex() const = 0;
    };
}
