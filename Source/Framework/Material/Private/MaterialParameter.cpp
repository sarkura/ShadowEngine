#include "Framework/Material/Public/MaterialParameter.h"

#include "Framework/Common/Public/Log.h"

#include <string_view>
#include <utility>

namespace ShadowEngine
{
    namespace
    {
        TextureAssetHandle FindTexture(const MaterialParameterBlock& Block, std::string_view Name)
        {
            const auto Found = Block.Textures.find(std::string(Name));
            return Found == Block.Textures.end() ? TextureAssetHandle{} : Found->second;
        }
    }

    void MaterialParameter::BuildLayout(
        const ShaderReflection& Reflection,
        MaterialParameterLayout& Layout)
    {
        MaterialParameterLayout Built;
        for (const ShaderBinding& Binding : Reflection.Bindings)
        {
            MaterialParameterLayout::Entry Entry;
            Entry.Name = Binding.Name;
            if (Binding.Kind == EShaderBindingKind::Float)
            {
                Entry.Type = EMaterialParameterType::Float;
            }
            else if (Binding.Kind == EShaderBindingKind::Float2)
            {
                Entry.Type = EMaterialParameterType::Float2;
            }
            else if (Binding.Kind == EShaderBindingKind::Float3)
            {
                Entry.Type = EMaterialParameterType::Float3;
            }
            else if (Binding.Kind == EShaderBindingKind::Float4)
            {
                Entry.Type = EMaterialParameterType::Float4;
            }
            else if (Binding.Kind == EShaderBindingKind::Texture2D)
            {
                Entry.Type = EMaterialParameterType::Texture2D;
            }
            else if (Binding.Kind == EShaderBindingKind::Sampler)
            {
                Entry.Type = EMaterialParameterType::Sampler;
            }
            else
            {
                continue;
            }

            Built.Entries.push_back(std::move(Entry));
        }

        Layout = std::move(Built);
    }

    bool MaterialParameter::HasResources(
        const MaterialParameterBlock& Block,
        const MaterialParameterLayout& Layout)
    {
        const auto ComponentCount = [](EMaterialParameterType Type) -> uint32
        {
            switch (Type)
            {
                case EMaterialParameterType::Float:
                    return 1;
                case EMaterialParameterType::Float2:
                    return 2;
                case EMaterialParameterType::Float3:
                    return 3;
                case EMaterialParameterType::Float4:
                    return 4;
                default:
                    return 0;
            }
        };
        for (const MaterialParameterLayout::Entry& Entry : Layout.Entries)
        {
            const uint32 Components = ComponentCount(Entry.Type);
            if (Components != 0)
            {
                const auto Found = Block.Scalars.find(Entry.Name);
                if (Found == Block.Scalars.end() || Found->second.Components != Components)
                {
                    return false;
                }
            }
            else if (Entry.Type == EMaterialParameterType::Texture2D && !FindTexture(Block, Entry.Name).IsValid())
            {
                return false;
            }
            else if (Entry.Type == EMaterialParameterType::Sampler && !Block.Sampler.IsValid())
            {
                return false;
            }
        }

        return true;
    }

    bool MaterialParameter::BuildBlock(
        const MaterialRendererBinding& Description,
        MaterialParameterBlock& Block,
        std::string* ErrorMessage)
    {
        MaterialParameterBlock Built;
        for (const MaterialScalarParameter& Scalar : Description.Scalars)
        {
            if (Scalar.Components != 1 && Scalar.Components != 2 && Scalar.Components != 3 && Scalar.Components != 4)
            {
                SetErrorMessage(ErrorMessage, "Material parameter '" + Scalar.Name + "' has an unsupported component count");
                return false;
            }

            MaterialScalarValue Value;
            Value.Components = Scalar.Components;
            for (uint32 Index = 0; Index < Scalar.Components; ++Index)
            {
                Value.Value[Index] = Scalar.Value[Index];
            }

            Built.Scalars.emplace(Scalar.Name, Value);
        }

        Block = Built;
        return true;
    }

    void MaterialParameter::SetTexture(
        MaterialParameterBlock& Block,
        const std::string& Name,
        TextureAssetHandle Handle)
    {
        Block.Textures[Name] = Handle;
    }

    void MaterialParameter::SetSampler(MaterialParameterBlock& Block, SamplerHandle Sampler)
    {
        Block.Sampler = Sampler;
    }

    MaterialParameter::TextureBinding MaterialParameter::GetTextureBinding(const MaterialParameterBlock& Block)
    {
        TextureBinding Binding;
        Binding.Textures = Block.Textures;
        Binding.Sampler = Block.Sampler;
        return Binding;
    }
}
