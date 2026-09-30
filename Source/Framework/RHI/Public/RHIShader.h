#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/RHI/Public/RHIEnums.h"

namespace ShadowEngine
{
    class RHIShader : public NonCopyable
    {
        public:
            virtual ~RHIShader() = default;

            [[nodiscard]] virtual ERHIShaderStage GetStage() const = 0;
    };
}
