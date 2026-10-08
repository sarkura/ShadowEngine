#pragma once

#include "Framework/RHI/Public/RHIDevice.h"
#include "RHI/Direct3D12/Public/D3D12Adapter.h"
#include "RHI/Direct3D12/Public/D3D12Common.h"
#include "RHI/Direct3D12/Public/D3D12Fence.h"

#include <memory>
#include <string>
#include <string_view>

namespace ShadowEngine
{
    class D3D12Device final : public RHIDevice
    {
        public:
            D3D12Device() = default;
            ~D3D12Device() override;

            bool Initialize(const RHIDeviceDesc& Desc, std::string* ErrorMessage = nullptr) override;
            void Finalize() override;

            [[nodiscard]] std::string_view GetName() const override;
            [[nodiscard]] ERHIShaderFormat GetShaderFormat() const override;
            [[nodiscard]] const RHIAdapter* GetAdapter() const override;

            std::unique_ptr<RHISwapChain> CreateSwapChain(
                const RHISwapChainDesc& Desc,
                std::string* ErrorMessage = nullptr) override;

            std::unique_ptr<RHIShader> CreateShader(
                const RHIShaderDesc& Desc,
                std::string* ErrorMessage = nullptr) override;

            std::unique_ptr<RHIPipeline> CreateGraphicsPipeline(
                const RHIGraphicsPipelineDesc& Desc,
                std::string* ErrorMessage = nullptr) override;

            std::unique_ptr<RHIBuffer> CreateVertexBuffer(
                const RHIBufferDesc& Desc,
                std::string* ErrorMessage = nullptr) override;

            std::unique_ptr<RHIBuffer> CreateIndexBuffer(
                const RHIBufferDesc& Desc,
                ERHIIndexFormat Format,
                std::string* ErrorMessage = nullptr) override;

            std::unique_ptr<RHIBuffer> CreateConstantBuffer(
                uint32 Size,
                std::string* ErrorMessage = nullptr) override;

            std::unique_ptr<RHITexture> CreateDepthTexture(
                uint32 Width,
                uint32 Height,
                std::string* ErrorMessage = nullptr) override;

            std::unique_ptr<RHITexture> CreateTexture(
                const RHITextureDesc& Desc,
                std::string* ErrorMessage = nullptr) override;

            std::unique_ptr<RHISampler> CreateSampler(
                std::string* ErrorMessage = nullptr) override;

            bool CreateShaderResourceView(
                RHITexture& Texture,
                uint32& OutIndex,
                std::string* ErrorMessage = nullptr) override;

            std::unique_ptr<RHICommandList> CreateCommandList(
                std::string* ErrorMessage = nullptr) override;

            void SubmitCommandList(RHICommandList& CommandList) override;
            void WaitIdle() override;

        private:
            bool CreateFactory(bool bEnableDebugLayer, std::string* ErrorMessage);
            bool CreateShaderVisibleHeap(
                D3D12_DESCRIPTOR_HEAP_TYPE Type,
                uint32 Capacity,
                ComPtr<ID3D12DescriptorHeap>& OutHeap,
                uint32& OutIncrement,
                std::string* ErrorMessage);
            void FlushDebugMessages();

            ComPtr<IDXGIFactory4> Factory;
            D3D12Adapter Adapter;
            ComPtr<ID3D12Device> Device;
            ComPtr<ID3D12InfoQueue> InfoQueue;
            ComPtr<ID3D12CommandQueue> Queue;
            std::unique_ptr<D3D12Fence> Fence;
            ComPtr<ID3D12DescriptorHeap> ResourceHeap;
            ComPtr<ID3D12DescriptorHeap> SamplerHeap;
            uint32 ResourceIncrement = 0;
            uint32 SamplerIncrement = 0;
            uint32 ResourceCount = 0;
            uint32 SamplerCount = 0;
    };
}
