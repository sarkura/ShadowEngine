#include "RHI/Direct3D12/Public/D3D12Fence.h"

#include "Framework/Common/Public/Log.h"

namespace ShadowEngine
{
    D3D12Fence::~D3D12Fence()
    {
        if (Event != nullptr)
        {
            CloseHandle(Event);
        }
    }

    bool D3D12Fence::Initialize(ID3D12Device* Device, std::string* ErrorMessage)
    {
        const HRESULT Result = Device->CreateFence(
            0,
            D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(Fence.GetAddressOf()));
        if (FAILED(Result))
        {
            SetErrorMessage(ErrorMessage, "CreateFence failed: " + FormatHResult(Result));
            return false;
        }

        Event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (Event == nullptr)
        {
            SetErrorMessage(ErrorMessage, "CreateEvent failed for the D3D12 fence");
            return false;
        }

        return true;
    }

    uint64 D3D12Fence::Signal(ID3D12CommandQueue* Queue)
    {
        const uint64 Value = NextValue++;
        Queue->Signal(Fence.Get(), Value);
        return Value;
    }

    uint64 D3D12Fence::GetCompletedValue() const
    {
        return Fence->GetCompletedValue();
    }

    void D3D12Fence::Wait(uint64 Value)
    {
        if (Fence->GetCompletedValue() >= Value)
        {
            return;
        }

        Fence->SetEventOnCompletion(Value, Event);
        WaitForSingleObject(Event, INFINITE);
    }
}
