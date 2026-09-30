#pragma once

#include "Framework/Engine/Public/Engine.h"
#include "Interface/Public/IApplication.hpp"

namespace ShadowEngine
{
    class BaseApplication : public IApplication
    {
        public:
            BaseApplication();
            ~BaseApplication() override;

            virtual int Initialize() override;
            virtual void Finalize() override;
            virtual void Tick(float DeltaTime) override;
            virtual bool IsQuit() override;

        protected:
            Engine EngineInstance;
            bool bQuit = false;
    };
}
