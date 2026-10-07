#pragma once

#include "Framework/Asset/Public/Asset.h"

#include <memory>
#include <string>
#include <vector>

namespace ShadowEngine
{
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

            [[nodiscard]] const std::vector<Mip>& GetMips() const;

        private:
            explicit TextureAsset(std::filesystem::path InPath, std::vector<Mip> InMips);

            std::vector<Mip> Mips;
    };
}
