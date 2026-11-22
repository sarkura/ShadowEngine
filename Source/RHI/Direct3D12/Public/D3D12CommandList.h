#pragma once

#include "Framework/RHI/Public/RHICommandList.h"
#include "RHI/Direct3D12/Public/D3D12Common.h"

#include <span>
#include <string>

namespace ShadowEngine
{
    class D3D12Pipeline;
    class D3D12Texture;

    class D3D12CommandList final : public RHICommandList
    {
        public:
            bool Initialize(
                ID3D12Device* Device,
                ID3D12DescriptorHeap* InResourceHeap,
                ID3D12DescriptorHeap* InSamplerHeap,
                std::string* ErrorMessage = nullptr);
            ~D3D12CommandList() override;

            void Begin() override;
            void End() override;

            void BeginRenderPass(
                std::span<RHITexture*> ColorTargets,
                RHITexture* DepthTarget,
                const RHIColor& ClearColor,
                bool bClearColor = true,
                bool bClearDepth = true,
                uint32 ArraySlice = 0) override;
            void EndRenderPass() override;
            void BeginDepthPass(RHITexture& DepthTarget, bool bClearDepth, uint32 ArraySlice = 0) override;
            void UseAsShaderResource(RHITexture& Texture) override;

            void SetPipeline(RHIPipeline& Pipeline) override;
            void SetVertexBuffer(RHIBuffer& Buffer) override;
            void SetIndexBuffer(RHIBuffer& Buffer) override;
            void SetConstantBuffer(RHIBuffer& Buffer, uint32 Offset) override;
            void BindShaderResources() override;
            void SetViewport(const RHIViewport& Viewport) override;
            void SetScissor(const RHIRect& Scissor) override;
            bool UploadTexture(RHITexture& Texture, std::span<const uint8> Pixels) override;
            void Draw(uint32 VertexCount, uint32 FirstVertex = 0) override;
            void DrawIndexed(uint32 IndexCount, uint32 FirstIndex = 0, int32 VertexOffset = 0) override;
            void ExecuteIndirect(RHIBuffer& Arguments, uint32 CommandCount) override;
            void DrawMeshlets(
                RHIBuffer& Vertices,
                RHIBuffer& VertexIds,
                RHIBuffer& PrimitiveIndices,
                RHIBuffer& Meshlets,
                uint32 MeshletCount) override;

            [[nodiscard]] ID3D12GraphicsCommandList* GetHandle() const;

        private:
            void Transition(D3D12Texture& Texture, D3D12_RESOURCE_STATES NewState);

            ComPtr<ID3D12CommandAllocator> Allocator;
            ComPtr<ID3D12GraphicsCommandList> CommandList;
            ComPtr<ID3D12GraphicsCommandList6> MeshCommands;
            ComPtr<ID3D12CommandSignature> CommandSignature;
            D3D12Pipeline* CurrentPipeline = nullptr;
            ComPtr<ID3D12Resource> TextureUpload;
            ID3D12Device* D3DDevice = nullptr;
            uint8* TextureUploadMapped = nullptr;
            UINT64 TextureUploadSize = 0;
            ID3D12DescriptorHeap* ResourceHeap = nullptr;
            ID3D12DescriptorHeap* SamplerHeap = nullptr;

            D3D12Texture* CurrentRenderTargets[8] = {};
            D3D12_RESOURCE_STATES RenderTargetRestoreStates[8] = {};
            uint32 CurrentRenderTargetCount = 0;
    };
}
