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
        MaterialParameterLayout Layout;
    };

    class Material final : public NonCopyable
    {
        public:
            explicit Material(std::vector<MaterialShaderBinding> InShaders);

            [[nodiscard]] const ShaderHandle* FindShader(std::string_view RendererName) const;
            [[nodiscard]] const MaterialParameterLayout* FindLayout(std::string_view RendererName) const;

        private:
            std::vector<MaterialShaderBinding> Shaders;
    };
}
