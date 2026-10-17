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

    enum class EMaterialSurface : uint8
    {
        Opaque,
        Cutout,
        Translucent
    };

    struct MaterialRendererBinding
    {
        std::string Renderer;
        std::string Parent;
        std::string VertexShader;
        std::string PixelShader;
        std::string MaterialShader;
        std::string LightShader;
        EMaterialSurface Surface = EMaterialSurface::Opaque;
        bool bTwoSided = false;
        bool bHasSurface = false;
        bool bHasTwoSided = false;
        bool bDefinesShader = false;
        bool bHasParameters = false;
        std::vector<MaterialScalarParameter> Scalars;
        std::vector<std::pair<std::string, std::string>> Textures;
        std::vector<MaterialScalarParameter> ShaderScalars;
        std::vector<std::pair<std::string, std::string>> ShaderTextures;
    };

    struct MaterialDescription
    {
        std::vector<MaterialRendererBinding> Bindings;
    };

    enum class ETextureEncoding : uint8
    {
        Linear,
        sRGB
    };

    struct TextureDescription
    {
        std::string Source;
        ETextureEncoding Encoding = ETextureEncoding::Linear;
    };
}
