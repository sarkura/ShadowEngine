#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Material/Public/MaterialParameterLayout.h"
#include "Framework/Shader/Public/ShaderHandle.h"

namespace ShadowEngine
{
    class Material final : public NonCopyable
    {
        public:
            Material(ShaderHandle InShader, MaterialParameterLayout InLayout);

            [[nodiscard]] const ShaderHandle& GetShader() const;
            [[nodiscard]] const MaterialParameterLayout& GetLayout() const;

        private:
            ShaderHandle Shader;
            MaterialParameterLayout Layout;
    };
}
