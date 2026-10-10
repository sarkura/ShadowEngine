#include "RHI/Direct3D12/Public/D3D12Texture.h"

#include "Framework/Common/Public/Log.h"
#include "RHI/Direct3D12/Public/D3D12Fence.h"

#include <cstring>
#include <utility>
#include <vector>

namespace ShadowEngine
{
    D3D12Texture::D3D12Texture(
        ComPtr<ID3D12Resource> InResource,
        uint32 InWidth,
        uint32 InHeight,
        ERHIFormat InFormat,
        D3D12_RESOURCE_STATES InitialState,
        D3D12_CPU_DESCRIPTOR_HANDLE InRenderTargetView,
        D3D12_CPU_DESCRIPTOR_HANDLE InDepthStencilView,
        ComPtr<ID3D12DescriptorHeap> InDescriptorHeap,
        uint32 InMipCount)
        : Resource(std::move(InResource))
        , DescriptorHeap(std::move(InDescriptorHeap))
        , Width(InWidth)
        , Height(InHeight)
        , MipCount(InMipCount == 0 ? 1 : InMipCount)
        , Format(InFormat)
        , State(InitialState)
        , RenderTargetView(InRenderTargetView)
        , DepthStencilView(InDepthStencilView)
    {
    }

    std::unique_ptr<D3D12Texture> D3D12Texture::CreateDepth(
        ID3D12Device* Device,
        uint32 InWidth,
        uint32 InHeight,
        std::string* ErrorMessage)
    {
        if (InWidth == 0 || InHeight == 0)
        {
            SetErrorMessage(ErrorMessage, "Depth texture requires a non-zero size");
            return nullptr;
        }

        D3D12_HEAP_PROPERTIES HeapProperties{};
        HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        HeapProperties.CreationNodeMask = 1;
        HeapProperties.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC ResourceDesc{};
        ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        ResourceDesc.Width = InWidth;
        ResourceDesc.Height = InHeight;
        ResourceDesc.DepthOrArraySize = 1;
        ResourceDesc.MipLevels = 1;
        ResourceDesc.Format = DXGI_FORMAT_D32_FLOAT;
        ResourceDesc.SampleDesc.Count = 1;
        ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        ResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE ClearValue{};
        ClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        ClearValue.DepthStencil.Depth = 1.0F;
        ClearValue.DepthStencil.Stencil = 0;

        ComPtr<ID3D12Resource> Resource;
        HRESULT Result = Device->CreateCommittedResource(
            &HeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &ResourceDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &ClearValue,
            IID_PPV_ARGS(Resource.GetAddressOf()));
        if (FAILED(Result))
        {
            SetErrorMessage(ErrorMessage, "CreateCommittedResource failed for the depth texture: " + FormatHResult(Result));
            return nullptr;
        }

        D3D12_DESCRIPTOR_HEAP_DESC HeapDesc{};
        HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        HeapDesc.NumDescriptors = 1;
        HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        ComPtr<ID3D12DescriptorHeap> Heap;
        Result = Device->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(Heap.GetAddressOf()));
        if (FAILED(Result))
        {
            SetErrorMessage(ErrorMessage, "CreateDescriptorHeap failed for the depth texture: " + FormatHResult(Result));
            return nullptr;
        }

        const D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView = Heap->GetCPUDescriptorHandleForHeapStart();
        D3D12_DEPTH_STENCIL_VIEW_DESC ViewDesc{};
        ViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
        ViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        Device->CreateDepthStencilView(Resource.Get(), &ViewDesc, DepthStencilView);

        return std::make_unique<D3D12Texture>(
            std::move(Resource),
            InWidth,
            InHeight,
            ERHIFormat::D32_Float,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            D3D12_CPU_DESCRIPTOR_HANDLE{},
            DepthStencilView,
            std::move(Heap));
    }

    std::unique_ptr<D3D12Texture> D3D12Texture::CreateSampled(
        ID3D12Device* Device,
        ID3D12CommandQueue* Queue,
        D3D12Fence& Fence,
        const RHITextureDesc& Desc,
        std::string* ErrorMessage)
    {
        const uint32 PixelStride = Desc.Format == ERHIFormat::R32G32B32A32_Float ? 16u : 4u;
        if (Desc.Mips.empty() ||
            (Desc.Format != ERHIFormat::R8G8B8A8_UNorm && Desc.Format != ERHIFormat::R32G32B32A32_Float))
        {
            SetErrorMessage(ErrorMessage, "Sampled texture format is unsupported");
            return nullptr;
        }

        const uint32 MipCount = static_cast<uint32>(Desc.Mips.size());
        D3D12_HEAP_PROPERTIES DefaultHeap{};
        DefaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
        DefaultHeap.CreationNodeMask = 1;
        DefaultHeap.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC ResourceDesc{};
        ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        ResourceDesc.Width = Desc.Mips[0].Width;
        ResourceDesc.Height = Desc.Mips[0].Height;
        ResourceDesc.DepthOrArraySize = 1;
        ResourceDesc.MipLevels = static_cast<UINT16>(MipCount);
        ResourceDesc.Format = ToDXGIFormat(Desc.Format);
        ResourceDesc.SampleDesc.Count = 1;
        ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

        ComPtr<ID3D12Resource> Resource;
        HRESULT Result = Device->CreateCommittedResource(
            &DefaultHeap,
            D3D12_HEAP_FLAG_NONE,
            &ResourceDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(Resource.GetAddressOf()));
        if (FAILED(Result))
        {
            SetErrorMessage(ErrorMessage, "CreateCommittedResource failed for the sampled texture: " + FormatHResult(Result));
            return nullptr;
        }

        std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> Layouts(MipCount);
        std::vector<UINT> Rows(MipCount);
        std::vector<UINT64> RowSizes(MipCount);
        UINT64 TotalSize = 0;
        Device->GetCopyableFootprints(
            &ResourceDesc,
            0,
            MipCount,
            0,
            Layouts.data(),
            Rows.data(),
            RowSizes.data(),
            &TotalSize);

        D3D12_HEAP_PROPERTIES UploadHeap{};
        UploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
        UploadHeap.CreationNodeMask = 1;
        UploadHeap.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC UploadDesc{};
        UploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        UploadDesc.Width = TotalSize;
        UploadDesc.Height = 1;
        UploadDesc.DepthOrArraySize = 1;
        UploadDesc.MipLevels = 1;
        UploadDesc.Format = DXGI_FORMAT_UNKNOWN;
        UploadDesc.SampleDesc.Count = 1;
        UploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ComPtr<ID3D12Resource> Upload;
        Result = Device->CreateCommittedResource(
            &UploadHeap,
            D3D12_HEAP_FLAG_NONE,
            &UploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(Upload.GetAddressOf()));
        if (FAILED(Result))
        {
            SetErrorMessage(ErrorMessage, "CreateCommittedResource failed for the texture upload: " + FormatHResult(Result));
            return nullptr;
        }

        uint8* Mapped = nullptr;
        Result = Upload->Map(0, nullptr, reinterpret_cast<void**>(&Mapped));
        if (FAILED(Result) || Mapped == nullptr)
        {
            SetErrorMessage(ErrorMessage, "Map failed for the texture upload: " + FormatHResult(Result));
            return nullptr;
        }

        for (uint32 MipIndex = 0; MipIndex < MipCount; ++MipIndex)
        {
            const RHITextureMipDesc& Mip = Desc.Mips[MipIndex];
            const uint32 SourcePitch = Mip.Width * PixelStride;
            if (Mip.Pixels == nullptr || Mip.Size < SourcePitch * Mip.Height)
            {
                Upload->Unmap(0, nullptr);
                SetErrorMessage(ErrorMessage, "Texture mip is smaller than its dimensions");
                return nullptr;
            }

            for (UINT Row = 0; Row < Rows[MipIndex]; ++Row)
            {
                std::memcpy(
                    Mapped + Layouts[MipIndex].Offset + static_cast<UINT64>(Row) * Layouts[MipIndex].Footprint.RowPitch,
                    Mip.Pixels + static_cast<size_t>(Row) * SourcePitch,
                    SourcePitch);
            }
        }
        Upload->Unmap(0, nullptr);

        ComPtr<ID3D12CommandAllocator> Allocator;
        ComPtr<ID3D12GraphicsCommandList> CommandList;
        Result = Device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(Allocator.GetAddressOf()));
        if (FAILED(Result))
        {
            SetErrorMessage(ErrorMessage, "CreateCommandAllocator failed for the texture upload: " + FormatHResult(Result));
            return nullptr;
        }

        Result = Device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            Allocator.Get(),
            nullptr,
            IID_PPV_ARGS(CommandList.GetAddressOf()));
        if (FAILED(Result))
        {
            SetErrorMessage(ErrorMessage, "CreateCommandList failed for the texture upload: " + FormatHResult(Result));
            return nullptr;
        }

        for (uint32 MipIndex = 0; MipIndex < MipCount; ++MipIndex)
        {
            D3D12_TEXTURE_COPY_LOCATION Destination{};
            Destination.pResource = Resource.Get();
            Destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            Destination.SubresourceIndex = MipIndex;

            D3D12_TEXTURE_COPY_LOCATION Source{};
            Source.pResource = Upload.Get();
            Source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            Source.PlacedFootprint = Layouts[MipIndex];
            CommandList->CopyTextureRegion(&Destination, 0, 0, 0, &Source, nullptr);
        }

        D3D12_RESOURCE_BARRIER Barrier{};
        Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        Barrier.Transition.pResource = Resource.Get();
        Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        CommandList->ResourceBarrier(1, &Barrier);
        CommandList->Close();

        ID3D12CommandList* Lists[] = {CommandList.Get()};
        Queue->ExecuteCommandLists(1, Lists);
        Fence.Wait(Fence.Signal(Queue));

        return std::make_unique<D3D12Texture>(
            std::move(Resource),
            Desc.Mips[0].Width,
            Desc.Mips[0].Height,
            Desc.Format,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_CPU_DESCRIPTOR_HANDLE{},
            D3D12_CPU_DESCRIPTOR_HANDLE{},
            ComPtr<ID3D12DescriptorHeap>{},
            MipCount);
    }

    uint32 D3D12Texture::GetWidth() const
    {
        return Width;
    }

    uint32 D3D12Texture::GetHeight() const
    {
        return Height;
    }

    uint32 D3D12Texture::GetMipCount() const
    {
        return MipCount;
    }

    ERHIFormat D3D12Texture::GetFormat() const
    {
        return Format;
    }

    ID3D12Resource* D3D12Texture::GetResource() const
    {
        return Resource.Get();
    }

    D3D12_RESOURCE_STATES D3D12Texture::GetState() const
    {
        return State;
    }

    void D3D12Texture::SetState(D3D12_RESOURCE_STATES NewState)
    {
        State = NewState;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE D3D12Texture::GetRenderTargetView() const
    {
        return RenderTargetView;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE D3D12Texture::GetDepthStencilView() const
    {
        return DepthStencilView;
    }

    bool D3D12Texture::HasDepthStencil() const
    {
        return DepthStencilView.ptr != 0;
    }
}
