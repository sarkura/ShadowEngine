#include "RHI/Direct3D12/Public/D3D12Shader.h"

namespace ShadowEngine
{
    D3D12Shader::D3D12Shader(const RHIShaderDesc& Desc)
        : Stage(Desc.Stage)
        , EntryPoint(Desc.EntryPoint)
        , Bytecode(Desc.Bytecode.begin(), Desc.Bytecode.end())
    {
    }

    ERHIShaderStage D3D12Shader::GetStage() const
    {
        return Stage;
    }

    D3D12_SHADER_BYTECODE D3D12Shader::GetBytecode() const
    {
        return {Bytecode.data(), Bytecode.size()};
    }
}
