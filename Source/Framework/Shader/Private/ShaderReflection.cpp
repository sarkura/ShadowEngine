#include "Framework/Shader/Public/ShaderReflection.h"

namespace ShadowEngine
{
    const ShaderBinding* ShaderReflection::Find(std::string_view Name, EShaderBindingKind Kind) const
    {
        for (const ShaderBinding& Binding : Bindings)
        {
            if (Binding.Kind == Kind && Binding.Name == Name)
            {
                return &Binding;
            }
        }

        return nullptr;
    }
}
