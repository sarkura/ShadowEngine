#pragma once

#include "Interface/Public/Interface.hpp"

namespace ShadowEngineInterface
{
    IInterface IRuntimeModule
    {
        public:
            virtual ~IRuntimeModule() = default;
            virtual int Initialize() = 0;
            virtual void Finalize() = 0;
            virtual void Tick(float DeltaTime) = 0;
    };
}
