#pragma once

#include "Framework/Common/Public/BaseApplication.h"

namespace ShadowEngine
{
    class EmptyApplication final : public BaseApplication
    {
        public:
            EmptyApplication();
            ~EmptyApplication() override;

            int Initialize() override;
    };
}
