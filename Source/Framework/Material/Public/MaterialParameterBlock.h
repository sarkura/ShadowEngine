#pragma once

#include "Framework/Asset/Public/AssetHandle.h"
#include "Framework/Common/Public/Types.h"

#include <array>
#include <map>
#include <string>

namespace ShadowEngine
{
    struct MaterialScalarValue
    {
        std::array<float, 4> Value{};
        uint32 Components = 1;
    };

    struct MaterialParameterBlock
    {
        std::map<std::string, MaterialScalarValue> Scalars;
        std::map<std::string, TextureAssetHandle> Textures;
        SamplerHandle Sampler;
    };
}
