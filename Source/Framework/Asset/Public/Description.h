#pragma once

#include "Framework/Common/Public/Types.h"

#include <string>
#include <utility>
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

    struct MaterialScalarParameter
    {
        std::string Name;
        uint32 Components = 1;
        float Value[4] = {};
    };

    struct MaterialRendererBinding
    {
        std::string Renderer;
        std::string VertexShader;
        std::string PixelShader;
        std::string MaterialShader;
        std::string LightShader;
        bool bHasParameters = false;
        std::vector<MaterialScalarParameter> Scalars;
        std::vector<std::pair<std::string, std::string>> Textures;
    };

    struct MaterialDescription
    {
        std::vector<MaterialRendererBinding> Bindings;
    };
}
