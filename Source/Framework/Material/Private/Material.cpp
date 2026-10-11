#include "Framework/Material/Public/Material.h"

#include <utility>

namespace ShadowEngine
{
    Material::Material(std::vector<MaterialShaderBinding> InShaders)
        : Shaders(std::move(InShaders))
    {
    }

    const ShaderHandle* Material::FindShader(std::string_view RendererName) const
    {
        for (const MaterialShaderBinding& Binding : Shaders)
        {
            if (Binding.Renderer == RendererName)
            {
                return &Binding.Shader;
            }
        }

        return nullptr;
    }
}
