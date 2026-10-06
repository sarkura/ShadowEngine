#pragma once

namespace ShadowEngine
{
    struct MaterialParameter
    {
        float BaseColor[3] = {1.0F, 1.0F, 1.0F};
        float Roughness = 0.5F;
        float SpecularColor[3] = {1.0F, 1.0F, 1.0F};
    };
}
