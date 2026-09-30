#pragma once

#include "Framework/RHI/Public/RHIDefinitions.h"
#include "Framework/RHI/Public/RHIPipeline.h"
#include "RHI/Direct3D12/Public/D3D12Common.h"

#include <string>

namespace ShadowEngine
{
    class D3D12Pipeline final : public RHIPipeline
    {
        public:
            bool Initialize(
                ID3D12Device* Device,
                const RHIGraphicsPipelineDesc& Desc,
                std::string* ErrorMessage = nullptr);

            [[nodiscard]] ID3D12RootSignature* GetRootSignature() const;
            [[nodiscard]] ID3D12PipelineState* GetPipelineState() const;
            [[nodiscard]] D3D_PRIMITIVE_TOPOLOGY GetTopology() const;

        private:
            bool CreateRootSignature(ID3D12Device* Device, std::string* ErrorMessage);

            ComPtr<ID3D12RootSignature> RootSignature;
            ComPtr<ID3D12PipelineState> PipelineState;
            D3D_PRIMITIVE_TOPOLOGY Topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    };
}
