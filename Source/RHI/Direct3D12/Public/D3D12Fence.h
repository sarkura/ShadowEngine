#pragma once

#include "Framework/RHI/Public/RHIFence.h"
#include "RHI/Direct3D12/Public/D3D12Common.h"

#include <string>

namespace ShadowEngine
{
    class D3D12Fence final : public RHIFence
    {
        public:
            D3D12Fence() = default;
            ~D3D12Fence() override;

            bool Initialize(ID3D12Device* Device, std::string* ErrorMessage = nullptr);

            uint64 Signal(ID3D12CommandQueue* Queue);
            [[nodiscard]] uint64 GetCompletedValue() const override;
            void Wait(uint64 Value) override;

        private:
            ComPtr<ID3D12Fence> Fence;
            HANDLE Event = nullptr;
            uint64 NextValue = 1;
    };
}
