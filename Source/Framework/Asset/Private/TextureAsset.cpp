#include "Framework/Asset/Public/TextureAsset.h"

#include "Framework/Common/Public/Log.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ShadowEngine
{
    namespace
    {
        float SrgbToLinearChannel(uint8 Encoded)
        {
            const float Value = static_cast<float>(Encoded) / 255.0f;
            if (Value <= 0.04045f)
            {
                return Value / 12.92f;
            }

            return std::pow((Value + 0.055f) / 1.055f, 2.4f);
        }

        uint8 LinearToSrgbChannel(float Linear)
        {
            const float Clamped = std::clamp(Linear, 0.0f, 1.0f);
            const float Encoded = Clamped <= 0.0031308f
                ? Clamped * 12.92f
                : 1.055f * std::pow(Clamped, 1.0f / 2.4f) - 0.055f;
            return static_cast<uint8>(std::clamp(Encoded, 0.0f, 1.0f) * 255.0f + 0.5f);
        }

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

        void DownsampleSrgb(
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

                    float Linear[3] = {};
                    uint32 Alpha = 0;
                    for (const uint32 Offset : Offsets)
                    {
                        Linear[0] += SrgbToLinearChannel(Source[Offset]);
                        Linear[1] += SrgbToLinearChannel(Source[Offset + 1]);
                        Linear[2] += SrgbToLinearChannel(Source[Offset + 2]);
                        Alpha += Source[Offset + 3];
                    }

                    uint8* Pixel = Dest.data() + (static_cast<size_t>(Y) * DestWidth + X) * 4;
                    Pixel[0] = LinearToSrgbChannel(Linear[0] * 0.25f);
                    Pixel[1] = LinearToSrgbChannel(Linear[1] * 0.25f);
                    Pixel[2] = LinearToSrgbChannel(Linear[2] * 0.25f);
                    Pixel[3] = static_cast<uint8>((Alpha + 2) / 4);
                }
            }
        }
    }

    std::unique_ptr<TextureAsset> TextureAsset::Create(
        std::filesystem::path InPath,
        uint32 Width,
        uint32 Height,
        std::vector<uint8> Pixels,
        ETextureEncoding Encoding,
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
            if (Encoding == ETextureEncoding::sRGB)
            {
                DownsampleSrgb(Mips.back().Pixels, Width, Height, NextWidth, NextHeight, Next.Pixels);
            }
            else
            {
                Downsample(Mips.back().Pixels, Width, Height, NextWidth, NextHeight, Next.Pixels);
            }
            Mips.push_back(std::move(Next));
            Width = NextWidth;
            Height = NextHeight;
        }

        return std::unique_ptr<TextureAsset>(
            new TextureAsset(std::move(InPath), std::move(Mips), ETextureFormat::RGBA8_UNorm, Encoding));
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
        return std::unique_ptr<TextureAsset>(
            new TextureAsset(std::move(InPath), std::move(Mips), ETextureFormat::RGBA32_Float, ETextureEncoding::Linear));
    }

    TextureAsset::TextureAsset(
        std::filesystem::path InPath,
        std::vector<Mip> InMips,
        ETextureFormat InFormat,
        ETextureEncoding InEncoding)
        : Asset(EAssetType::Texture, std::move(InPath))
        , Mips(std::move(InMips))
        , Format(InFormat)
        , Encoding(InEncoding)
    {
    }

    ETextureFormat TextureAsset::GetFormat() const
    {
        return Format;
    }

    ETextureEncoding TextureAsset::GetEncoding() const
    {
        return Encoding;
    }

    const std::vector<TextureAsset::Mip>& TextureAsset::GetMips() const
    {
        return Mips;
    }
}
