#pragma once

#include "Framework/RHI/Public/RHIAdapter.h"
#include "RHI/Direct3D12/Public/D3D12Common.h"

#include <string>

namespace ShadowEngine
{
    class D3D12Adapter final : public RHIAdapter
    {
        public:
            bool Initialize(IDXGIFactory4* Factory, std::string* ErrorMessage = nullptr);

            [[nodiscard]] const RHIAdapterInfo& GetInfo() const override;
            [[nodiscard]] IDXGIAdapter1* GetHandle() const;

        private:
            bool TrySelect(ComPtr<IDXGIAdapter1> Candidate);

            ComPtr<IDXGIAdapter1> Adapter;
            RHIAdapterInfo Info;
    };
}
