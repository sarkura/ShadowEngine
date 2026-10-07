#pragma once

#include "Framework/RHI/Public/RHISampler.h"
#include "RHI/Direct3D12/Public/D3D12Common.h"

#include <memory>
#include <string>

namespace ShadowEngine
{
    class D3D12Sampler final : public RHISampler
    {
        public:
            [[nodiscard]] static std::unique_ptr<D3D12Sampler> Create(
                ID3D12Device* Device,
                std::string* ErrorMessage = nullptr);

            [[nodiscard]] ID3D12DescriptorHeap* GetHeap() const;
            [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle() const;

        private:
            ComPtr<ID3D12DescriptorHeap> Heap;
            D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle{};
    };
}
