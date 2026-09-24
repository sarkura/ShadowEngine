#pragma once

#include "Interface/Public/Interface.hpp"
#include "Interface/Public/IRuntimeModule.hpp"

namespace ShadowEngineInterface
{
    IInterface IApplication :  IImplements IRuntimeModule
    {
        public:
            virtual ~IApplication() = default;
            virtual int Initialize() = 0;
            virtual void Finalize() = 0;
            virtual void Tick(float DeltaTime) = 0;
            virtual bool IsQuit() = 0;
    };
};
