#include "RHI/Direct3D12/Public/D3D12CommandList.h"

#include "Framework/Common/Public/Log.h"
#include "RHI/Direct3D12/Public/D3D12Pipeline.h"
#include "RHI/Direct3D12/Public/D3D12Texture.h"

namespace ShadowEngine
{
    bool D3D12CommandList::Initialize(ID3D12Device* Device, std::string* ErrorMessage)
    {
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
        return true;
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

    void D3D12CommandList::BeginRenderPass(RHITexture& RenderTarget, const RHIColor& ClearColor)
    {
        auto& Texture = static_cast<D3D12Texture&>(RenderTarget);
        RenderTargetRestoreState = Texture.GetState();
        Transition(Texture, D3D12_RESOURCE_STATE_RENDER_TARGET);
        CurrentRenderTarget = &Texture;

        const D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView = Texture.GetRenderTargetView();
        const float Color[4] = {ClearColor.R, ClearColor.G, ClearColor.B, ClearColor.A};
        CommandList->OMSetRenderTargets(1, &RenderTargetView, FALSE, nullptr);
        CommandList->ClearRenderTargetView(RenderTargetView, Color, 0, nullptr);
    }

    void D3D12CommandList::EndRenderPass()
    {
        if (CurrentRenderTarget != nullptr)
        {
            Transition(*CurrentRenderTarget, RenderTargetRestoreState);
            CurrentRenderTarget = nullptr;
        }
    }

    void D3D12CommandList::SetPipeline(RHIPipeline& Pipeline)
    {
        auto& D3D12PipelineObject = static_cast<D3D12Pipeline&>(Pipeline);
        CommandList->SetGraphicsRootSignature(D3D12PipelineObject.GetRootSignature());
        CommandList->SetPipelineState(D3D12PipelineObject.GetPipelineState());
        CommandList->IASetPrimitiveTopology(D3D12PipelineObject.GetTopology());
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
