#include "RHI/Direct3D12/Public/D3D12Texture.h"

#include <utility>

namespace ShadowEngine
{
    D3D12Texture::D3D12Texture(
        ComPtr<ID3D12Resource> InResource,
        uint32 InWidth,
        uint32 InHeight,
        ERHIFormat InFormat,
        D3D12_RESOURCE_STATES InitialState,
        D3D12_CPU_DESCRIPTOR_HANDLE InRenderTargetView)
        : Resource(std::move(InResource))
        , Width(InWidth)
        , Height(InHeight)
        , Format(InFormat)
        , State(InitialState)
        , RenderTargetView(InRenderTargetView)
    {
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
}
