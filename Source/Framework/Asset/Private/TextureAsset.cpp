#include "Framework/Asset/Public/TextureAsset.h"

#include "Framework/Common/Public/Log.h"

#include <algorithm>
#include <utility>

namespace ShadowEngine
{
    namespace
    {
        void Downsample(
            const std::vector<uint8>& Source,
            uint32 SourceWidth,
            uint32 SourceHeight,
            uint32 DestWidth,
            uint32 DestHeight,
            std::vector<uint8>& Dest)
        {
            Dest.resize(static_cast<size_t>(DestWidth) * DestHeight * 4);
            for (uint32 Y = 0; Y < DestHeight; ++Y)
            {
                for (uint32 X = 0; X < DestWidth; ++X)
                {
                    const uint32 X0 = X * 2;
                    const uint32 Y0 = Y * 2;
                    const uint32 X1 = std::min(X0 + 1, SourceWidth - 1);
                    const uint32 Y1 = std::min(Y0 + 1, SourceHeight - 1);
                    const uint32 Offsets[4] = {
                        (Y0 * SourceWidth + X0) * 4,
                        (Y0 * SourceWidth + X1) * 4,
                        (Y1 * SourceWidth + X0) * 4,
                        (Y1 * SourceWidth + X1) * 4,
                    };

                    uint8* Pixel = Dest.data() + (static_cast<size_t>(Y) * DestWidth + X) * 4;
                    for (uint32 Channel = 0; Channel < 4; ++Channel)
                    {
                        const uint32 Sum = static_cast<uint32>(Source[Offsets[0] + Channel]) +
                            Source[Offsets[1] + Channel] +
                            Source[Offsets[2] + Channel] +
                            Source[Offsets[3] + Channel];
                        Pixel[Channel] = static_cast<uint8>((Sum + 2) / 4);
                    }
                }
            }
        }
    }

    std::unique_ptr<TextureAsset> TextureAsset::Create(
        std::filesystem::path InPath,
        uint32 Width,
        uint32 Height,
        std::vector<uint8> Pixels,
        std::string* ErrorMessage)
    {
        if (Width == 0 || Height == 0 || Pixels.size() != static_cast<size_t>(Width) * Height * 4)
        {
            SetErrorMessage(ErrorMessage, "Texture pixels do not match its size: " + InPath.generic_string());
            return nullptr;
        }

        std::vector<Mip> Mips;
        Mip First;
        First.Width = Width;
        First.Height = Height;
        First.Pixels = std::move(Pixels);
        Mips.push_back(std::move(First));

        while (Width > 1 || Height > 1)
        {
            const uint32 NextWidth = std::max<uint32>(1, Width / 2);
            const uint32 NextHeight = std::max<uint32>(1, Height / 2);
            Mip Next;
            Next.Width = NextWidth;
            Next.Height = NextHeight;
            Downsample(Mips.back().Pixels, Width, Height, NextWidth, NextHeight, Next.Pixels);
            Mips.push_back(std::move(Next));
            Width = NextWidth;
            Height = NextHeight;
        }

        return std::unique_ptr<TextureAsset>(new TextureAsset(std::move(InPath), std::move(Mips), ETextureFormat::RGBA8_UNorm));
    }

    std::unique_ptr<TextureAsset> TextureAsset::CreateFloat(
        std::filesystem::path InPath,
        uint32 Width,
        uint32 Height,
        std::vector<uint8> Pixels,
        std::string* ErrorMessage)
    {
        if (Width == 0 || Height == 0 || Pixels.size() != static_cast<size_t>(Width) * Height * 16)
        {
            SetErrorMessage(ErrorMessage, "Texture pixels do not match its size: " + InPath.generic_string());
            return nullptr;
        }

        Mip First;
        First.Width = Width;
        First.Height = Height;
        First.Pixels = std::move(Pixels);
        std::vector<Mip> Mips;
        Mips.push_back(std::move(First));
        return std::unique_ptr<TextureAsset>(new TextureAsset(std::move(InPath), std::move(Mips), ETextureFormat::RGBA32_Float));
    }

    TextureAsset::TextureAsset(std::filesystem::path InPath, std::vector<Mip> InMips, ETextureFormat InFormat)
        : Asset(EAssetType::Texture, std::move(InPath))
        , Mips(std::move(InMips))
        , Format(InFormat)
    {
    }

    ETextureFormat TextureAsset::GetFormat() const
    {
        return Format;
    }

    const std::vector<TextureAsset::Mip>& TextureAsset::GetMips() const
    {
        return Mips;
    }
}
