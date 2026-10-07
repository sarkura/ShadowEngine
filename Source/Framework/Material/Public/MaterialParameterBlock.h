#pragma once

#include "Framework/Asset/Public/AssetHandle.h"

namespace ShadowEngine
{
    struct MaterialParameterBlock
    {
        float BaseColor[4] = {1.0F, 1.0F, 1.0F, 1.0F};
        float Roughness[4] = {0.5F, 0.0F, 0.0F, 0.0F};
        float SpecularColor[4] = {1.0F, 1.0F, 1.0F, 1.0F};
        TextureAssetHandle BaseColorTexture;
        TextureAssetHandle RoughnessTexture;
        SamplerHandle Sampler;
    };
}
