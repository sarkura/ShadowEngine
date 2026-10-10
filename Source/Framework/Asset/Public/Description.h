#pragma once

#include "Framework/Common/Public/Types.h"

#include <string>
#include <vector>

namespace ShadowEngine
{
    struct MeshMaterialBinding
    {
        uint32 Slot = 0;
        std::string Material;
    };

    struct MeshDescription
    {
        std::string Source;
        std::vector<MeshMaterialBinding> Materials;
    };

    struct MaterialRendererBinding
    {
        std::string Renderer;
        std::string VertexShader;
        std::string PixelShader;
        std::string MaterialShader;
        std::string LightShader;
        bool bHasParameters = false;
        std::vector<float> BaseColor;
        float Roughness = 0.0F;
        std::vector<float> SpecularColor;
        std::string BaseColorTexture;
        std::string RoughnessTexture;
        std::string NormalTexture;
    };

    struct MaterialDescription
    {
        std::vector<MaterialRendererBinding> Bindings;
    };
}
