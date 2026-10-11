#pragma once

#include "Framework/Common/Public/Types.h"
#include "Framework/RHI/Public/RHIEnums.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ShadowEngine
{
    struct ShaderScalarField
    {
        std::string Name;
        uint32 Components = 0;

        bool operator==(const ShaderScalarField& Other) const
        {
            return Name == Other.Name && Components == Other.Components;
        }
    };

    struct ShaderCompileRequest
    {
        std::filesystem::path SourcePath;
        std::filesystem::path ImplementationPath;
        std::filesystem::path LightPath;
        std::string EntryPoint;
        std::vector<std::string> DescriptorFields;
        std::vector<ShaderScalarField> ScalarFields;
        ERHIShaderStage Stage = ERHIShaderStage::Vertex;
        ERHIShaderFormat Format = ERHIShaderFormat::DXBC;
    };

    struct ShaderBytecode
    {
        ERHIShaderStage Stage = ERHIShaderStage::Vertex;
        std::string EntryPoint;
        std::vector<uint8> Data;
    };
}
