#include "RHI/Direct3D12/Public/D3D12Texture.h"

#include "Framework/Common/Public/Log.h"

#include <utility>

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
        ComPtr<ID3D12DescriptorHeap> InDescriptorHeap)
        : Resource(std::move(InResource))
        , DescriptorHeap(std::move(InDescriptorHeap))
        , Width(InWidth)
        , Height(InHeight)
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

    uint32 D3D12Texture::GetWidth() const
    {
        return Width;
    }

    uint32 D3D12Texture::GetHeight() const
    {
        return Height;
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
