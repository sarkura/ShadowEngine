#include "RHI/Direct3D12/Public/D3D12CommandList.h"

#include "Framework/Common/Public/Log.h"
#include "RHI/Direct3D12/Public/D3D12Buffer.h"
#include "RHI/Direct3D12/Public/D3D12Pipeline.h"
#include "RHI/Direct3D12/Public/D3D12Texture.h"

#include <cstring>

namespace ShadowEngine
{
    bool D3D12CommandList::Initialize(
        ID3D12Device* Device,
        ID3D12DescriptorHeap* InResourceHeap,
        ID3D12DescriptorHeap* InSamplerHeap,
        std::string* ErrorMessage)
    {
        D3DDevice = Device;
        ResourceHeap = InResourceHeap;
        SamplerHeap = InSamplerHeap;
        HRESULT Result = Device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(Allocator.GetAddressOf()));
        if (FAILED(Result))
        {
            SetErrorMessage(ErrorMessage, "CreateCommandAllocator failed: " + FormatHResult(Result));
            return false;
        }

        Result = Device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            Allocator.Get(),
            nullptr,
            IID_PPV_ARGS(CommandList.GetAddressOf()));
        if (FAILED(Result))
        {
            SetErrorMessage(ErrorMessage, "CreateCommandList failed: " + FormatHResult(Result));
            return false;
        }

        CommandList->Close();

        D3D12_INDIRECT_ARGUMENT_DESC Argument{};
        Argument.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
        D3D12_COMMAND_SIGNATURE_DESC Signature{};
        Signature.ByteStride = sizeof(RHIDrawIndexedArguments);
        Signature.NumArgumentDescs = 1;
        Signature.pArgumentDescs = &Argument;
        Result = Device->CreateCommandSignature(&Signature, nullptr, IID_PPV_ARGS(CommandSignature.GetAddressOf()));
        if (FAILED(Result))
        {
            SetErrorMessage(ErrorMessage, "CreateCommandSignature failed: " + FormatHResult(Result));
            return false;
        }

        static_assert(sizeof(RHIDrawIndexedArguments) == sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));
        return true;
    }

    D3D12CommandList::~D3D12CommandList()
    {
        if (TextureUpload != nullptr && TextureUploadMapped != nullptr)
        {
            TextureUpload->Unmap(0, nullptr);
            TextureUploadMapped = nullptr;
        }
    }

    void D3D12CommandList::Begin()
    {
        Allocator->Reset();
        CommandList->Reset(Allocator.Get(), nullptr);
    }

    void D3D12CommandList::End()
    {
        CommandList->Close();
    }

    void D3D12CommandList::BeginRenderPass(
        std::span<RHITexture*> ColorTargets,
        RHITexture* DepthTarget,
        const RHIColor& ClearColor,
        bool bClearColor,
        bool bClearDepth,
        uint32 ArraySlice)
    {
        CurrentRenderTargetCount = static_cast<uint32>(ColorTargets.size());
        if (CurrentRenderTargetCount > 8)
        {
            CurrentRenderTargetCount = 8;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetViews[8] = {};
        for (uint32 Index = 0; Index < CurrentRenderTargetCount; ++Index)
        {
            auto& Texture = static_cast<D3D12Texture&>(*ColorTargets[Index]);
            RenderTargetRestoreStates[Index] = Texture.GetState();
            Transition(Texture, D3D12_RESOURCE_STATE_RENDER_TARGET);
            CurrentRenderTargets[Index] = &Texture;
            RenderTargetViews[Index] = Texture.GetRenderTargetView(ArraySlice);
        }

        D3D12Texture* DepthTexture = nullptr;
        if (DepthTarget != nullptr)
        {
            DepthTexture = &static_cast<D3D12Texture&>(*DepthTarget);
            Transition(*DepthTexture, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        }

        const D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView =
            DepthTexture != nullptr ? DepthTexture->GetDepthStencilView() : D3D12_CPU_DESCRIPTOR_HANDLE{};
        const D3D12_CPU_DESCRIPTOR_HANDLE* DepthStencil =
            DepthTexture != nullptr && DepthTexture->HasDepthStencil() ? &DepthStencilView : nullptr;
        CommandList->OMSetRenderTargets(
            CurrentRenderTargetCount,
            CurrentRenderTargetCount > 0 ? RenderTargetViews : nullptr,
            FALSE,
            DepthStencil);
        if (bClearColor)
        {
            const float Color[4] = {ClearColor.R, ClearColor.G, ClearColor.B, ClearColor.A};
            for (uint32 Index = 0; Index < CurrentRenderTargetCount; ++Index)
            {
                CommandList->ClearRenderTargetView(RenderTargetViews[Index], Color, 0, nullptr);
            }
        }
        if (bClearDepth && DepthStencil != nullptr)
        {
            CommandList->ClearDepthStencilView(
                *DepthStencil,
                D3D12_CLEAR_FLAG_DEPTH,
                1.0F,
                0,
                0,
                nullptr);
        }
    }

    void D3D12CommandList::BeginDepthPass(RHITexture& DepthTarget, bool bClearDepth, uint32 ArraySlice)
    {
        auto& Texture = static_cast<D3D12Texture&>(DepthTarget);
        Transition(Texture, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        const D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView = Texture.GetDepthStencilView(ArraySlice);
        CommandList->OMSetRenderTargets(0, nullptr, FALSE, &DepthStencilView);
        if (bClearDepth)
        {
            CommandList->ClearDepthStencilView(
                DepthStencilView,
                D3D12_CLEAR_FLAG_DEPTH,
                1.0F,
                0,
                0,
                nullptr);
        }
    }

    void D3D12CommandList::UseAsShaderResource(RHITexture& Texture)
    {
        Transition(static_cast<D3D12Texture&>(Texture), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

    void D3D12CommandList::EndRenderPass()
    {
        for (uint32 Index = 0; Index < CurrentRenderTargetCount; ++Index)
        {
            if (CurrentRenderTargets[Index] != nullptr)
            {
                Transition(*CurrentRenderTargets[Index], RenderTargetRestoreStates[Index]);
                CurrentRenderTargets[Index] = nullptr;
            }
        }
        CurrentRenderTargetCount = 0;
    }

    void D3D12CommandList::SetPipeline(RHIPipeline& Pipeline)
    {
        auto& D3D12PipelineObject = static_cast<D3D12Pipeline&>(Pipeline);
        ID3D12DescriptorHeap* Heaps[] = {ResourceHeap, SamplerHeap};
        CommandList->SetDescriptorHeaps(2, Heaps);
        CommandList->SetGraphicsRootSignature(D3D12PipelineObject.GetRootSignature());
        CommandList->SetPipelineState(D3D12PipelineObject.GetPipelineState());
        CommandList->IASetPrimitiveTopology(D3D12PipelineObject.GetTopology());
    }

    void D3D12CommandList::SetVertexBuffer(RHIBuffer& Buffer)
    {
        const D3D12_VERTEX_BUFFER_VIEW View = static_cast<D3D12Buffer&>(Buffer).GetVertexBufferView();
        CommandList->IASetVertexBuffers(0, 1, &View);
    }

    void D3D12CommandList::SetIndexBuffer(RHIBuffer& Buffer)
    {
        const D3D12_INDEX_BUFFER_VIEW View = static_cast<D3D12Buffer&>(Buffer).GetIndexBufferView();
        CommandList->IASetIndexBuffer(&View);
    }

    void D3D12CommandList::SetConstantBuffer(RHIBuffer& Buffer, uint32 Offset)
    {
        const D3D12_GPU_VIRTUAL_ADDRESS Address =
            static_cast<D3D12Buffer&>(Buffer).GetGPUVirtualAddress() + Offset;
        CommandList->SetGraphicsRootConstantBufferView(0, Address);
    }

    void D3D12CommandList::BindShaderResources()
    {
        ID3D12DescriptorHeap* Heaps[] = {ResourceHeap, SamplerHeap};
        CommandList->SetDescriptorHeaps(2, Heaps);
    }

    bool D3D12CommandList::UploadTexture(RHITexture& Texture, std::span<const uint8> Pixels)
    {
        auto& Target = static_cast<D3D12Texture&>(Texture);
        if (D3DDevice == nullptr || CommandList == nullptr || Target.GetResource() == nullptr ||
            (Target.GetFormat() != ERHIFormat::R8G8B8A8_UNorm && Target.GetFormat() != ERHIFormat::R8G8B8A8_UNorm_sRGB))
        {
            return false;
        }

        const uint32 Width = Target.GetWidth();
        const uint32 Height = Target.GetHeight();
        const uint32 SourcePitch = Width * 4;
        if (Width == 0 || Height == 0 || Pixels.size() < static_cast<size_t>(SourcePitch) * Height)
        {
            return false;
        }

        const D3D12_RESOURCE_DESC ResourceDesc = Target.GetResource()->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT Layout{};
        UINT Rows = 0;
        UINT64 RowSize = 0;
        UINT64 TotalSize = 0;
        D3DDevice->GetCopyableFootprints(&ResourceDesc, 0, 1, 0, &Layout, &Rows, &RowSize, &TotalSize);
        if (TotalSize == 0 || Rows < Height)
        {
            return false;
        }

        if (TextureUpload == nullptr || TextureUploadSize < TotalSize)
        {
            if (TextureUpload != nullptr && TextureUploadMapped != nullptr)
            {
                TextureUpload->Unmap(0, nullptr);
                TextureUploadMapped = nullptr;
            }
            TextureUpload.Reset();

            D3D12_HEAP_PROPERTIES Heap{};
            Heap.Type = D3D12_HEAP_TYPE_UPLOAD;
            Heap.CreationNodeMask = 1;
            Heap.VisibleNodeMask = 1;
            D3D12_RESOURCE_DESC UploadDesc{};
            UploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            UploadDesc.Width = TotalSize;
            UploadDesc.Height = 1;
            UploadDesc.DepthOrArraySize = 1;
            UploadDesc.MipLevels = 1;
            UploadDesc.Format = DXGI_FORMAT_UNKNOWN;
            UploadDesc.SampleDesc.Count = 1;
            UploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            const HRESULT Created = D3DDevice->CreateCommittedResource(
                &Heap,
                D3D12_HEAP_FLAG_NONE,
                &UploadDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(TextureUpload.GetAddressOf()));
            if (FAILED(Created))
            {
                return false;
            }

            void* Mapped = nullptr;
            if (FAILED(TextureUpload->Map(0, nullptr, &Mapped)) || Mapped == nullptr)
            {
                TextureUpload.Reset();
                return false;
            }
            TextureUploadMapped = static_cast<uint8*>(Mapped);
            TextureUploadSize = TotalSize;
        }

        for (UINT Row = 0; Row < Height; ++Row)
        {
            std::memcpy(
                TextureUploadMapped + Layout.Offset + static_cast<UINT64>(Row) * Layout.Footprint.RowPitch,
                Pixels.data() + static_cast<size_t>(Row) * SourcePitch,
                SourcePitch);
        }

        Transition(Target, D3D12_RESOURCE_STATE_COPY_DEST);
        D3D12_TEXTURE_COPY_LOCATION Destination{};
        Destination.pResource = Target.GetResource();
        Destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        Destination.SubresourceIndex = 0;
        D3D12_TEXTURE_COPY_LOCATION Source{};
        Source.pResource = TextureUpload.Get();
        Source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        Source.PlacedFootprint = Layout;
        CommandList->CopyTextureRegion(&Destination, 0, 0, 0, &Source, nullptr);
        Transition(Target, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        return true;
    }

    void D3D12CommandList::SetViewport(const RHIViewport& Viewport)
    {
        const D3D12_VIEWPORT D3D12Viewport{
            Viewport.X,
            Viewport.Y,
            Viewport.Width,
            Viewport.Height,
            Viewport.MinDepth,
            Viewport.MaxDepth};
        CommandList->RSSetViewports(1, &D3D12Viewport);
    }

    void D3D12CommandList::SetScissor(const RHIRect& Scissor)
    {
        const D3D12_RECT Rect{Scissor.Left, Scissor.Top, Scissor.Right, Scissor.Bottom};
        CommandList->RSSetScissorRects(1, &Rect);
    }

    void D3D12CommandList::Draw(uint32 VertexCount, uint32 FirstVertex)
    {
        CommandList->DrawInstanced(VertexCount, 1, FirstVertex, 0);
    }

    void D3D12CommandList::DrawIndexed(uint32 IndexCount, uint32 FirstIndex, int32 VertexOffset)
    {
        CommandList->DrawIndexedInstanced(IndexCount, 1, FirstIndex, VertexOffset, 0);
    }

    void D3D12CommandList::ExecuteIndirect(RHIBuffer& Arguments, uint32 CommandCount)
    {
        if (CommandCount == 0 || CommandSignature == nullptr)
        {
            return;
        }

        CommandList->ExecuteIndirect(
            CommandSignature.Get(),
            CommandCount,
            static_cast<D3D12Buffer&>(Arguments).GetResource(),
            0,
            nullptr,
            0);
    }

    ID3D12GraphicsCommandList* D3D12CommandList::GetHandle() const
    {
        return CommandList.Get();
    }

    void D3D12CommandList::Transition(D3D12Texture& Texture, D3D12_RESOURCE_STATES NewState)
    {
        const D3D12_RESOURCE_STATES OldState = Texture.GetState();
        if (OldState == NewState)
        {
            return;
        }

        D3D12_RESOURCE_BARRIER Barrier{};
        Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        Barrier.Transition.pResource = Texture.GetResource();
        Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        Barrier.Transition.StateBefore = OldState;
        Barrier.Transition.StateAfter = NewState;
        CommandList->ResourceBarrier(1, &Barrier);

        Texture.SetState(NewState);
    }
}
