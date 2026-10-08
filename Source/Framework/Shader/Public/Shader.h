#pragma once

#include "Framework/RHI/Public/RHIEnums.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ShadowEngine
{
    struct ShaderCompileRequest
    {
        std::filesystem::path SourcePath;
        std::filesystem::path ImplementationPath;
        std::string EntryPoint;
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
