#pragma once

#include "Framework/Common/Public/Types.h"

#include <string>
#include <string_view>
#include <vector>

namespace ShadowEngine
{
    enum class EShaderBindingKind : uint8
    {
        ConstantBuffer,
        Float,
        Float2,
        Float3,
        Float4,
        Uniform,
        Texture2D,
        Sampler,
    };

    struct ShaderBinding
    {
        std::string Name;
        EShaderBindingKind Kind = EShaderBindingKind::Float4;
        uint32 Slot = 0;
        uint32 Space = 0;
        uint32 Offset = 0;
        uint32 Size = 0;
    };

    struct ShaderReflection
    {
        std::vector<ShaderBinding> Bindings;

        [[nodiscard]] const ShaderBinding* Find(std::string_view Name, EShaderBindingKind Kind) const;
    };
}
