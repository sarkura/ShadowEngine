#pragma once

#include "Framework/RHI/Public/RHIDefinitions.h"
#include "Framework/RHI/Public/RHIShader.h"
#include "RHI/Direct3D12/Public/D3D12Common.h"

#include <string>
#include <vector>

namespace ShadowEngine
{
    class D3D12Shader final : public RHIShader
    {
        public:
            explicit D3D12Shader(const RHIShaderDesc& Desc);

            [[nodiscard]] ERHIShaderStage GetStage() const override;
            [[nodiscard]] D3D12_SHADER_BYTECODE GetBytecode() const;

        private:
            ERHIShaderStage Stage;
            std::string EntryPoint;
            std::vector<uint8> Bytecode;
    };
}
