#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "RHI/Direct3D12/Public/D3D12Common.h"

#include <string>

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

        private:
            ComPtr<ID3D12DescriptorHeap> Heap;
            D3D12_CPU_DESCRIPTOR_HANDLE Start{};
            uint32 IncrementSize = 0;
            uint32 Capacity = 0;
            uint32 Count = 0;
    };
}
