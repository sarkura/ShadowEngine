#pragma once

#include "Framework/Common/Public/NonCopyable.h"

namespace ShadowEngine
{
    class RHIPipeline : public NonCopyable
    {
        public:
            virtual ~RHIPipeline() = default;
    };
}
