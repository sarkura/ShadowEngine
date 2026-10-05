#pragma once

#include "Framework/RHI/Public/RHICommandList.h"
#include "RHI/Direct3D12/Public/D3D12Common.h"

#include <string>

namespace ShadowEngine
{
    class D3D12Texture;

    class D3D12CommandList final : public RHICommandList
    {
        public:
            bool Initialize(ID3D12Device* Device, std::string* ErrorMessage = nullptr);

            void Begin() override;
            void End() override;

            void BeginRenderPass(
                RHITexture& RenderTarget,
                RHITexture* DepthTarget,
                const RHIColor& ClearColor) override;
            void EndRenderPass() override;

            void SetPipeline(RHIPipeline& Pipeline) override;
            void SetVertexBuffer(RHIBuffer& Buffer) override;
            void SetIndexBuffer(RHIBuffer& Buffer) override;
            void SetConstantBuffer(RHIBuffer& Buffer, uint32 Offset) override;
            void SetViewport(const RHIViewport& Viewport) override;
            void SetScissor(const RHIRect& Scissor) override;
            void Draw(uint32 VertexCount, uint32 FirstVertex = 0) override;
            void DrawIndexed(uint32 IndexCount, uint32 FirstIndex = 0, int32 VertexOffset = 0) override;

            [[nodiscard]] ID3D12GraphicsCommandList* GetHandle() const;

        private:
            void Transition(D3D12Texture& Texture, D3D12_RESOURCE_STATES NewState);

            ComPtr<ID3D12CommandAllocator> Allocator;
            ComPtr<ID3D12GraphicsCommandList> CommandList;

            D3D12Texture* CurrentRenderTarget = nullptr;
            D3D12_RESOURCE_STATES RenderTargetRestoreState = D3D12_RESOURCE_STATE_COMMON;
    };
}
