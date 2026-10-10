#pragma once

#include "Framework/Asset/Public/Asset.h"

#include <memory>
#include <string>
#include <vector>

namespace ShadowEngine
{
    enum class ETextureFormat : uint8
    {
        RGBA8_UNorm,
        RGBA32_Float
    };

    class TextureAsset final : public Asset
    {
        public:
            struct Mip
            {
                uint32 Width = 0;
                uint32 Height = 0;
                std::vector<uint8> Pixels;
            };

            static std::unique_ptr<TextureAsset> Create(
                std::filesystem::path InPath,
                uint32 Width,
                uint32 Height,
                std::vector<uint8> Pixels,
                std::string* ErrorMessage = nullptr);

            static std::unique_ptr<TextureAsset> CreateFloat(
                std::filesystem::path InPath,
                uint32 Width,
                uint32 Height,
                std::vector<uint8> Pixels,
                std::string* ErrorMessage = nullptr);

            [[nodiscard]] ETextureFormat GetFormat() const;
            [[nodiscard]] const std::vector<Mip>& GetMips() const;

        private:
            TextureAsset(std::filesystem::path InPath, std::vector<Mip> InMips, ETextureFormat InFormat);

            std::vector<Mip> Mips;
            ETextureFormat Format = ETextureFormat::RGBA8_UNorm;
    };
}
