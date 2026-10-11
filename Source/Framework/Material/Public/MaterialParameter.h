#pragma once

#include "Framework/Asset/Public/Description.h"
#include "Framework/Material/Public/MaterialParameterBlock.h"
#include "Framework/Material/Public/MaterialParameterLayout.h"
#include "Framework/Shader/Public/ShaderReflection.h"

#include <map>
#include <string>

namespace ShadowEngine
{
    class MaterialParameter final
    {
        public:
            MaterialParameter() = delete;

            static void BuildLayout(
                const ShaderReflection& Reflection,
                MaterialParameterLayout& Layout);

            [[nodiscard]] static bool HasResources(
                const MaterialParameterBlock& Block,
                const MaterialParameterLayout& Layout);

            static bool BuildBlock(
                const MaterialRendererBinding& Description,
                MaterialParameterBlock& Block,
                std::string* ErrorMessage = nullptr);

            static void SetTexture(
                MaterialParameterBlock& Block,
                const std::string& Name,
                TextureAssetHandle Handle);

            static void SetSampler(
                MaterialParameterBlock& Block,
                SamplerHandle Sampler);

            struct TextureBinding
            {
                std::map<std::string, TextureAssetHandle> Textures;
                SamplerHandle Sampler;

                [[nodiscard]] bool HasTextures() const
                {
                    for (const auto& [Name, Handle] : Textures)
                    {
                        (void)Name;
                        if (Handle.IsValid())
                        {
                            return true;
                        }
                    }

                    return false;
                }

                [[nodiscard]] bool IsValid() const
                {
                    if (!HasTextures())
                    {
                        return !Sampler.IsValid();
                    }

                    return Sampler.IsValid();
                }
            };

            [[nodiscard]] static TextureBinding GetTextureBinding(const MaterialParameterBlock& Block);
    };
}
