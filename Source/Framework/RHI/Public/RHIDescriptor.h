#pragma once

#include "Framework/Common/Public/NonCopyable.h"

namespace ShadowEngine
{
    class RHIMaterialBinding : public NonCopyable
    {
        public:
            virtual ~RHIMaterialBinding() = default;
    };
}
