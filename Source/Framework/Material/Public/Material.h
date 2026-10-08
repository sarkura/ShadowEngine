#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Material/Public/MaterialParameterLayout.h"
#include "Framework/Shader/Public/ShaderHandle.h"

#include <string>
#include <string_view>
#include <vector>

namespace ShadowEngine
{
    struct MaterialShaderBinding
    {
        std::string Renderer;
        ShaderHandle Shader;
    };

    class Material final : public NonCopyable
    {
        public:
            Material(std::vector<MaterialShaderBinding> InShaders, MaterialParameterLayout InLayout);

            [[nodiscard]] const ShaderHandle* FindShader(std::string_view RendererName) const;
            [[nodiscard]] const MaterialParameterLayout& GetLayout() const;

        private:
            std::vector<MaterialShaderBinding> Shaders;
            MaterialParameterLayout Layout;
    };
}
