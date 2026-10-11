#pragma once

#include "Framework/Shader/Public/ShaderReflection.h"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace ShadowEngine
{
    constexpr uint32 InvalidShaderDescriptor = 0xffffffff;

    struct RHIPipelineLayout
    {
        struct ConstantBuffer
        {
            uint32 Slot = 0;
            uint32 Space = 0;
            uint32 Size = 0;
        };

        struct Descriptor
        {
            std::string Name;
            uint32 Offset = 0;
        };

        struct Scalar
        {
            std::string Name;
            uint32 Offset = 0;
            uint32 Components = 0;
        };

        struct Uniform
        {
            std::string Name;
            uint32 Offset = 0;
        };

        std::vector<ConstantBuffer> ConstantBuffers;
        std::vector<Descriptor> Descriptors;
        std::vector<Scalar> Scalars;
        std::vector<Uniform> Uniforms;

        static RHIPipelineLayout FromReflection(
            const ShaderReflection& Vertex,
            const ShaderReflection& Pixel);

        void WriteDescriptors(
            uint8* Destination,
            const std::map<std::string, uint32>& Textures,
            uint32 Sampler) const;

        void WriteScalars(
            uint8* Destination,
            const std::map<std::string, std::array<float, 4>>& Values) const;

        void WriteUniform(
            uint8* Destination,
            std::string_view Name,
            const float Value[4]) const;
    };
}
