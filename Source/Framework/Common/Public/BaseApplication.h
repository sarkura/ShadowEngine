#pragma once

#include "Interface/Public/IApplication.hpp"

namespace ShadowEngineInterface
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
            bool bQuit = false;
    };
}