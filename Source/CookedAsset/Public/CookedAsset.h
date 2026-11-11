#pragma once

#include "CookedAsset/Public/MeshVertex.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ShadowEngine
{
    enum class ECookedTextureFormat : uint8
    {
        RGBA8_UNorm,
        RGBA32_Float
    };

    enum class ECookedTextureEncoding : uint8
    {
        Linear,
        sRGB
    };

    struct CookedMeshSection
    {
        uint32 Slot = 0;
        bool bHasTexCoords = false;
        std::vector<MeshVertex> Vertices;
        std::vector<uint32> Indices;
    };

    struct CookedMesh
    {
        std::vector<CookedMeshSection> Sections;
    };

    struct CookedTextureMip
    {
        uint32 Width = 0;
        uint32 Height = 0;
        std::vector<uint8> Pixels;
    };

    struct CookedTexture
    {
        ECookedTextureFormat Format = ECookedTextureFormat::RGBA8_UNorm;
        ECookedTextureEncoding Encoding = ECookedTextureEncoding::Linear;
        std::vector<CookedTextureMip> Mips;
    };

    [[nodiscard]] std::filesystem::path CookedMeshPath(const std::filesystem::path& Source);
    [[nodiscard]] std::filesystem::path CookedTexturePath(const std::filesystem::path& Source);

    [[nodiscard]] bool SaveCookedMesh(const std::filesystem::path& Path, const CookedMesh& Mesh, std::string* ErrorMessage);
    [[nodiscard]] bool LoadCookedMesh(const std::filesystem::path& Path, CookedMesh& Mesh, std::string* ErrorMessage);
    [[nodiscard]] bool SaveCookedTexture(const std::filesystem::path& Path, const CookedTexture& Texture, std::string* ErrorMessage);
    [[nodiscard]] bool LoadCookedTexture(const std::filesystem::path& Path, CookedTexture& Texture, std::string* ErrorMessage);
}
