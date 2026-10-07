#include "Framework/Material/Public/Material.h"

#include <utility>

namespace ShadowEngine
{
    Material::Material(ShaderHandle InShader, MaterialParameterLayout InLayout)
        : Shader(std::move(InShader))
        , Layout(std::move(InLayout))
    {
    }

    const ShaderHandle& Material::GetShader() const
    {
        return Shader;
    }

    const MaterialParameterLayout& Material::GetLayout() const
    {
        return Layout;
    }
}
