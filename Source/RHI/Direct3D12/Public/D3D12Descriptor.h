#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/RHI/Public/RHIDescriptor.h"
#include "RHI/Direct3D12/Public/D3D12Common.h"

#include <string>

namespace ShadowEngine
{
    class D3D12Sampler;
    class D3D12Texture;
}

namespace ShadowEngine
{
    class D3D12DescriptorHeap final : public NonCopyable
    {
        public:
            bool Initialize(
                ID3D12Device* Device,
                D3D12_DESCRIPTOR_HEAP_TYPE Type,
                uint32 Capacity,
                std::string* ErrorMessage = nullptr);

            bool Allocate(D3D12_CPU_DESCRIPTOR_HANDLE& Handle);
            void Reset();

            [[nodiscard]] ID3D12DescriptorHeap* GetHeap() const;

        private:
            ComPtr<ID3D12DescriptorHeap> Heap;
            D3D12_CPU_DESCRIPTOR_HANDLE Start{};
            uint32 IncrementSize = 0;
            uint32 Capacity = 0;
            uint32 Count = 0;
    };

    class D3D12MaterialBinding final : public RHIMaterialBinding
    {
        public:
            bool Initialize(
                ID3D12Device* Device,
                D3D12Texture& BaseColor,
                D3D12Texture& Roughness,
                D3D12Sampler& Sampler,
                std::string* ErrorMessage = nullptr);

            [[nodiscard]] ID3D12DescriptorHeap* GetSrvHeap() const;
            [[nodiscard]] ID3D12DescriptorHeap* GetSamplerHeap() const;
            [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpu() const;
            [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetSamplerGpu() const;

        private:
            ComPtr<ID3D12DescriptorHeap> SrvHeap;
            ID3D12DescriptorHeap* SamplerHeap = nullptr;
            D3D12_GPU_DESCRIPTOR_HANDLE SrvGpu{};
            D3D12_GPU_DESCRIPTOR_HANDLE SamplerGpu{};
    };
}
