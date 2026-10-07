#pragma once

#include "Framework/Common/Public/NonCopyable.h"

namespace ShadowEngine
{
    class RHISampler : public NonCopyable
    {
        public:
            virtual ~RHISampler() = default;
    };
}
