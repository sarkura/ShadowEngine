#include "Framework/Material/Public/MaterialParameter.h"

#include "Framework/Common/Public/Log.h"

#include <utility>

namespace ShadowEngine
{
    bool MaterialParameter::BuildLayout(
        const MaterialDescription& Description,
        MaterialParameterLayout& Layout,
        std::string* ErrorMessage)
    {
        if (Description.BaseColorTexture.empty() != Description.RoughnessTexture.empty())
        {
            SetErrorMessage(ErrorMessage, "BaseColorTexture and RoughnessTexture must be set together");
            return false;
        }

        MaterialParameterLayout Built;
        Built.Entries.push_back({"BaseColor", EMaterialParameterType::Float4});
        Built.Entries.push_back({"Roughness", EMaterialParameterType::Float4});
        Built.Entries.push_back({"SpecularColor", EMaterialParameterType::Float4});
        if (!Description.BaseColorTexture.empty())
        {
            Built.Entries.push_back({"BaseColorTexture", EMaterialParameterType::Texture2D});
            Built.Entries.push_back({"RoughnessTexture", EMaterialParameterType::Texture2D});
            Built.Entries.push_back({"Sampler", EMaterialParameterType::Sampler});
        }

        Layout = std::move(Built);
        return true;
    }

    bool MaterialParameter::BuildBlock(
        const MaterialDescription& Description,
        MaterialParameterBlock& Block,
        std::string* ErrorMessage)
    {
        if (Description.BaseColor.size() != 3 || Description.SpecularColor.size() != 3)
        {
            SetErrorMessage(ErrorMessage, "Material colors must contain 3 numbers");
            return false;
        }

        if (Description.Roughness < 0.0F || Description.Roughness > 1.0F)
        {
            SetErrorMessage(ErrorMessage, "Roughness must be between 0 and 1");
            return false;
        }

        MaterialParameterBlock Built;
        Built.BaseColor[0] = Description.BaseColor[0];
        Built.BaseColor[1] = Description.BaseColor[1];
        Built.BaseColor[2] = Description.BaseColor[2];
        Built.BaseColor[3] = 1.0F;
        Built.Roughness[0] = Description.Roughness;
        Built.SpecularColor[0] = Description.SpecularColor[0];
        Built.SpecularColor[1] = Description.SpecularColor[1];
        Built.SpecularColor[2] = Description.SpecularColor[2];
        Built.SpecularColor[3] = 1.0F;
        Block = Built;
        return true;
    }

    void MaterialParameter::SetTextures(
        MaterialParameterBlock& Block,
        TextureAssetHandle BaseColor,
        TextureAssetHandle Roughness,
        SamplerHandle Sampler)
    {
        Block.BaseColorTexture = BaseColor;
        Block.RoughnessTexture = Roughness;
        Block.Sampler = Sampler;
    }

    void MaterialParameter::WriteSurfaceConstants(
        const MaterialParameterBlock& Block,
        SurfaceConstants& Out)
    {
        SurfaceConstants Packed;
        Packed.BaseColor[0] = Block.BaseColor[0];
        Packed.BaseColor[1] = Block.BaseColor[1];
        Packed.BaseColor[2] = Block.BaseColor[2];
        Packed.BaseColor[3] = Block.BaseColor[3];
        Packed.SpecularColor[0] = Block.SpecularColor[0];
        Packed.SpecularColor[1] = Block.SpecularColor[1];
        Packed.SpecularColor[2] = Block.SpecularColor[2];
        Packed.SpecularColor[3] = Block.SpecularColor[3];
        Packed.Roughness[0] = Block.Roughness[0];
        Packed.Roughness[1] = Block.Roughness[1];
        Packed.Roughness[2] = Block.Roughness[2];
        Packed.Roughness[3] = Block.Roughness[3];
        Out = Packed;
    }

    MaterialParameter::TextureBinding MaterialParameter::GetTextureBinding(const MaterialParameterBlock& Block)
    {
        TextureBinding Binding;
        Binding.BaseColor = Block.BaseColorTexture;
        Binding.Roughness = Block.RoughnessTexture;
        Binding.Sampler = Block.Sampler;
        return Binding;
    }
}
