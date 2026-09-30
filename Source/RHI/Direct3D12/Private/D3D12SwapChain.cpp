#include "RHI/Direct3D12/Public/D3D12SwapChain.h"

#include "Framework/Common/Public/Log.h"

namespace ShadowEngine
{
    bool D3D12SwapChain::Initialize(
        IDXGIFactory4* Factory,
        ID3D12Device* InDevice,
        ID3D12CommandQueue* Queue,
        const RHISwapChainDesc& InDesc,
        std::string* ErrorMessage)
    {
        if (InDesc.WindowHandle == nullptr || InDesc.Width == 0 || InDesc.Height == 0)
        {
            SetErrorMessage(ErrorMessage, "Swap chain requires a window and a non-zero size");
            return false;
        }

        Desc = InDesc;
        const HWND Window = static_cast<HWND>(Desc.WindowHandle);

        DXGI_SWAP_CHAIN_DESC1 SwapChainDesc{};
        SwapChainDesc.Width = Desc.Width;
        SwapChainDesc.Height = Desc.Height;
        SwapChainDesc.Format = ToDXGIFormat(Desc.Format);
        SwapChainDesc.SampleDesc.Count = 1;
        SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        SwapChainDesc.BufferCount = Desc.BufferCount;
        SwapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        SwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

        ComPtr<IDXGISwapChain1> SwapChain1;
        HRESULT Result = Factory->CreateSwapChainForHwnd(
            Queue,
            Window,
            &SwapChainDesc,
            nullptr,
            nullptr,
            SwapChain1.GetAddressOf());
        if (FAILED(Result))
        {
            SetErrorMessage(ErrorMessage, "CreateSwapChainForHwnd failed: " + FormatHResult(Result));
            return false;
        }

        Factory->MakeWindowAssociation(Window, DXGI_MWA_NO_ALT_ENTER);

        Result = SwapChain1.As(&SwapChain);
        if (FAILED(Result))
        {
            SetErrorMessage(ErrorMessage, "IDXGISwapChain3 is not supported: " + FormatHResult(Result));
            return false;
        }

        if (!RenderTargetHeap.Initialize(
                InDevice,
                D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
                Desc.BufferCount,
                ErrorMessage))
        {
            return false;
        }

        Device = InDevice;
        return CreateBackBuffers(ErrorMessage);
    }

    bool D3D12SwapChain::CreateBackBuffers(std::string* ErrorMessage)
    {
        RenderTargetHeap.Reset();
        BackBuffers.clear();
        BackBuffers.reserve(Desc.BufferCount);
        for (uint32 Index = 0; Index < Desc.BufferCount; ++Index)
        {
            ComPtr<ID3D12Resource> Buffer;
            const HRESULT Result = SwapChain->GetBuffer(Index, IID_PPV_ARGS(Buffer.GetAddressOf()));
            if (FAILED(Result))
            {
                SetErrorMessage(ErrorMessage, "IDXGISwapChain::GetBuffer failed: " + FormatHResult(Result));
                return false;
            }

            D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView{};
            RenderTargetHeap.Allocate(RenderTargetView);
            Device->CreateRenderTargetView(Buffer.Get(), nullptr, RenderTargetView);

            BackBuffers.push_back(std::make_unique<D3D12Texture>(
                std::move(Buffer),
                Desc.Width,
                Desc.Height,
                Desc.Format,
                D3D12_RESOURCE_STATE_PRESENT,
                RenderTargetView));
        }

        return true;
    }

    RHITexture& D3D12SwapChain::GetCurrentBackBuffer()
    {
        return *BackBuffers[SwapChain->GetCurrentBackBufferIndex()];
    }

    uint32 D3D12SwapChain::GetWidth() const
    {
        return Desc.Width;
    }

    uint32 D3D12SwapChain::GetHeight() const
    {
        return Desc.Height;
    }

    ERHIFormat D3D12SwapChain::GetFormat() const
    {
        return Desc.Format;
    }

    bool D3D12SwapChain::Present()
    {
        const HRESULT Result = SwapChain->Present(Desc.bVSync ? 1 : 0, 0);
        if (FAILED(Result))
        {
            Log::Error("IDXGISwapChain::Present failed: {}", FormatHResult(Result));
            return false;
        }

        return true;
    }

    bool D3D12SwapChain::Resize(uint32 Width, uint32 Height, std::string* ErrorMessage)
    {
        if (Width == 0 || Height == 0)
        {
            SetErrorMessage(ErrorMessage, "Swap chain cannot be resized to a zero size");
            return false;
        }

        if (Width == Desc.Width && Height == Desc.Height)
        {
            return true;
        }

        BackBuffers.clear();

        const HRESULT Result = SwapChain->ResizeBuffers(
            Desc.BufferCount,
            Width,
            Height,
            ToDXGIFormat(Desc.Format),
            0);
        if (FAILED(Result))
        {
            SetErrorMessage(ErrorMessage, "IDXGISwapChain::ResizeBuffers failed: " + FormatHResult(Result));
            return false;
        }

        Desc.Width = Width;
        Desc.Height = Height;
        return CreateBackBuffers(ErrorMessage);
    }
}
