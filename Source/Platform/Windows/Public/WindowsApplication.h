#pragma once

#include "Framework/Common/Public/BaseApplication.h"

namespace ShadowEngine
{
    class WindowsApplication final : public BaseApplication
    {
        public:
            WindowsApplication();
            ~WindowsApplication() override;

            int Initialize() override;
            void Finalize() override;
            void Tick(float DeltaTime) override;

        private:
            void* WindowHandle = nullptr;
            void* ModuleHandle = nullptr;
    };
}