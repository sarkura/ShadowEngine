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

            void OnWindowResized(uint32 Width, uint32 Height);

        private:
            void* WindowHandle = nullptr;
            void* ModuleHandle = nullptr;
    };
}