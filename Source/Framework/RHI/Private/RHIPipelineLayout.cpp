#include "Framework/RHI/Public/RHIPipelineLayout.h"

#include <array>
#include <cstring>

namespace ShadowEngine
{
    namespace
    {
        void AppendConstantBuffers(const ShaderReflection& Reflection, RHIPipelineLayout& Layout)
        {
            for (const ShaderBinding& Binding : Reflection.Bindings)
            {
                if (Binding.Kind != EShaderBindingKind::ConstantBuffer)
                {
                    continue;
                }

                bool bExists = false;
                for (const RHIPipelineLayout::ConstantBuffer& Existing : Layout.ConstantBuffers)
                {
                    if (Existing.Slot == Binding.Slot && Existing.Space == Binding.Space)
                    {
                        bExists = true;
                        break;
                    }
                }
                if (bExists)
                {
                    continue;
                }

                RHIPipelineLayout::ConstantBuffer Buffer;
                Buffer.Slot = Binding.Slot;
                Buffer.Space = Binding.Space;
                Buffer.Size = Binding.Size;
                Layout.ConstantBuffers.push_back(Buffer);
            }
        }

        void AppendScalars(const ShaderReflection& Reflection, RHIPipelineLayout& Layout)
        {
            for (const ShaderBinding& Binding : Reflection.Bindings)
            {
                uint32 Components = 0;
                if (Binding.Kind == EShaderBindingKind::Float)
                {
                    Components = 1;
                }
                else if (Binding.Kind == EShaderBindingKind::Float2)
                {
                    Components = 2;
                }
                else if (Binding.Kind == EShaderBindingKind::Float3)
                {
                    Components = 3;
                }
                else if (Binding.Kind == EShaderBindingKind::Float4)
                {
                    Components = 4;
                }
                else
                {
                    continue;
                }

                RHIPipelineLayout::Scalar Item;
                Item.Name = Binding.Name;
                Item.Offset = Binding.Offset;
                Item.Components = Components;
                Layout.Scalars.push_back(std::move(Item));
            }
        }

        void AppendUniforms(const ShaderReflection& Reflection, RHIPipelineLayout& Layout)
        {
            for (const ShaderBinding& Binding : Reflection.Bindings)
            {
                if (Binding.Kind != EShaderBindingKind::Uniform)
                {
                    continue;
                }

                RHIPipelineLayout::Uniform Item;
                Item.Name = Binding.Name;
                Item.Offset = Binding.Offset;
                Layout.Uniforms.push_back(std::move(Item));
            }
        }

        void AppendDescriptors(const ShaderReflection& Reflection, RHIPipelineLayout& Layout)
        {
            for (const ShaderBinding& Binding : Reflection.Bindings)
            {
                if (Binding.Kind != EShaderBindingKind::Texture2D && Binding.Kind != EShaderBindingKind::Sampler)
                {
                    continue;
                }

                RHIPipelineLayout::Descriptor Item;
                Item.Name = Binding.Name;
                Item.Offset = Binding.Offset;
                Layout.Descriptors.push_back(Item);
            }
        }
    }

    RHIPipelineLayout RHIPipelineLayout::FromReflection(
        const ShaderReflection& Vertex,
        const ShaderReflection& Pixel)
    {
        RHIPipelineLayout Layout;
        AppendConstantBuffers(Vertex, Layout);
        AppendConstantBuffers(Pixel, Layout);
        AppendScalars(Pixel, Layout);
        AppendUniforms(Pixel, Layout);
        AppendDescriptors(Pixel, Layout);
        return Layout;
    }

    void RHIPipelineLayout::WriteDescriptors(
        uint8* Destination,
        const std::map<std::string, uint32>& Textures,
        uint32 Sampler) const
    {
        if (Destination == nullptr)
        {
            return;
        }

        for (const Descriptor& Item : Descriptors)
        {
            uint32 Index = InvalidShaderDescriptor;
            if (Item.Name == "Sampler")
            {
                Index = Sampler;
            }
            else if (const auto Found = Textures.find(Item.Name); Found != Textures.end())
            {
                Index = Found->second;
            }

            std::memcpy(Destination + Item.Offset, &Index, sizeof(Index));
        }
    }

    void RHIPipelineLayout::WriteScalars(
        uint8* Destination,
        const std::map<std::string, std::array<float, 4>>& Values) const
    {
        if (Destination == nullptr)
        {
            return;
        }

        for (const Scalar& Item : Scalars)
        {
            std::array<float, 4> Value{};
            if (const auto Found = Values.find(Item.Name); Found != Values.end())
            {
                Value = Found->second;
            }

            std::memcpy(Destination + Item.Offset, Value.data(), sizeof(float) * Item.Components);
        }
    }

    void RHIPipelineLayout::WriteUniform(uint8* Destination, std::string_view Name, const float Value[4]) const
    {
        if (Destination == nullptr || Value == nullptr)
        {
            return;
        }

        for (const Uniform& Item : Uniforms)
        {
            if (Item.Name == Name)
            {
                std::memcpy(Destination + Item.Offset, Value, sizeof(float) * 4);
                return;
            }
        }
    }
}
