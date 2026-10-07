#pragma once

#include "Framework/Asset/Public/Description.h"
#include "Framework/Material/Public/MaterialParameterBlock.h"
#include "Framework/Material/Public/MaterialParameterLayout.h"

#include <string>

namespace ShadowEngine
{
    class MaterialParameter final
    {
        public:
            MaterialParameter() = delete;

            static bool BuildLayout(
                const MaterialDescription& Description,
                MaterialParameterLayout& Layout,
                std::string* ErrorMessage = nullptr);

            static bool BuildBlock(
                const MaterialDescription& Description,
                MaterialParameterBlock& Block,
                std::string* ErrorMessage = nullptr);

            static void SetTextures(
                MaterialParameterBlock& Block,
                TextureAssetHandle BaseColor,
                TextureAssetHandle Roughness,
                SamplerHandle Sampler);

            struct SurfaceConstants
            {
                float BaseColor[4] = {};
                float SpecularColor[4] = {};
                float Roughness[4] = {};
            };

            static void WriteSurfaceConstants(
                const MaterialParameterBlock& Block,
                SurfaceConstants& Out);

            struct TextureBinding
            {
                TextureAssetHandle BaseColor;
                TextureAssetHandle Roughness;
                SamplerHandle Sampler;

                [[nodiscard]] bool HasTextures() const
                {
                    return BaseColor.IsValid();
                }

                [[nodiscard]] bool IsValid() const
                {
                    if (BaseColor.IsValid() != Roughness.IsValid())
                    {
                        return false;
                    }

                    if (!BaseColor.IsValid())
                    {
                        return !Sampler.IsValid();
                    }

                    return Sampler.IsValid();
                }
            };

            [[nodiscard]] static TextureBinding GetTextureBinding(const MaterialParameterBlock& Block);
    };
}
