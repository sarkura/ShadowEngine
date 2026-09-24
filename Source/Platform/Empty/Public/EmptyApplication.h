#pragma once

#include "Framework/Common/Public/BaseApplication.h"

namespace ShadowEngineInterface
{
    class EmptyApplication final : public BaseApplication
    {
        public:
            EmptyApplication();
            ~EmptyApplication() override;
    };
}
