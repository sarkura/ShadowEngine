#pragma once

#include "Framework/RHI/Public/RHIDefinitions.h"
#include "Framework/RHI/Public/RHISwapChain.h"
#include "RHI/Direct3D12/Public/D3D12Common.h"
#include "RHI/Direct3D12/Public/D3D12Descriptor.h"
#include "RHI/Direct3D12/Public/D3D12Texture.h"

#include <memory>
#include <string>
#include <vector>

namespace ShadowEngine
{
    class D3D12SwapChain final : public RHISwapChain
    {
        public:
            bool Initialize(
                IDXGIFactory4* Factory,
                ID3D12Device* Device,
                ID3D12CommandQueue* Queue,
                const RHISwapChainDesc& InDesc,
                std::string* ErrorMessage = nullptr);

            [[nodiscard]] RHITexture& GetCurrentBackBuffer() override;
            [[nodiscard]] uint32 GetWidth() const override;
            [[nodiscard]] uint32 GetHeight() const override;
            [[nodiscard]] ERHIFormat GetFormat() const override;

            bool Present() override;
            bool Resize(uint32 Width, uint32 Height, std::string* ErrorMessage = nullptr) override;

        private:
            bool CreateBackBuffers(std::string* ErrorMessage);

            RHISwapChainDesc Desc;
            ComPtr<ID3D12Device> Device;
            ComPtr<IDXGISwapChain3> SwapChain;
            D3D12DescriptorHeap RenderTargetHeap;
            std::vector<std::unique_ptr<D3D12Texture>> BackBuffers;
    };
}
