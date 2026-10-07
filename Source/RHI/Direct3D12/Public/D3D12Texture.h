#pragma once

#include "Framework/RHI/Public/RHITexture.h"
#include "RHI/Direct3D12/Public/D3D12Common.h"

#include "Framework/RHI/Public/RHIDefinitions.h"

#include <memory>
#include <string>

namespace ShadowEngine
{
    class D3D12Fence;

    class D3D12Texture final : public RHITexture
    {
        public:
            D3D12Texture(
                ComPtr<ID3D12Resource> InResource,
                uint32 InWidth,
                uint32 InHeight,
                ERHIFormat InFormat,
                D3D12_RESOURCE_STATES InitialState,
                D3D12_CPU_DESCRIPTOR_HANDLE InRenderTargetView,
                D3D12_CPU_DESCRIPTOR_HANDLE InDepthStencilView = {},
                ComPtr<ID3D12DescriptorHeap> InDescriptorHeap = {},
                uint32 InMipCount = 1);

            [[nodiscard]] static std::unique_ptr<D3D12Texture> CreateSampled(
                ID3D12Device* Device,
                ID3D12CommandQueue* Queue,
                D3D12Fence& Fence,
                const RHITextureDesc& Desc,
                std::string* ErrorMessage = nullptr);

            [[nodiscard]] static std::unique_ptr<D3D12Texture> CreateDepth(
                ID3D12Device* Device,
                uint32 InWidth,
                uint32 InHeight,
                std::string* ErrorMessage = nullptr);

            [[nodiscard]] uint32 GetWidth() const override;
            [[nodiscard]] uint32 GetHeight() const override;
            [[nodiscard]] uint32 GetMipCount() const override;
            [[nodiscard]] ERHIFormat GetFormat() const override;

            [[nodiscard]] ID3D12Resource* GetResource() const;
            [[nodiscard]] D3D12_RESOURCE_STATES GetState() const;
            void SetState(D3D12_RESOURCE_STATES NewState);
            [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView() const;
            [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const;
            [[nodiscard]] bool HasDepthStencil() const;

        private:
            ComPtr<ID3D12Resource> Resource;
            ComPtr<ID3D12DescriptorHeap> DescriptorHeap;
            uint32 Width = 0;
            uint32 Height = 0;
            uint32 MipCount = 1;
            ERHIFormat Format = ERHIFormat::Unknown;
            D3D12_RESOURCE_STATES State = D3D12_RESOURCE_STATE_COMMON;
            D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView{};
            D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView{};
    };
}
