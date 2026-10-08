#include "RHI/Direct3D12/Public/D3D12Descriptor.h"

#include "Framework/Common/Public/Log.h"

namespace ShadowEngine
{
    bool D3D12DescriptorHeap::Initialize(
        ID3D12Device* Device,
        D3D12_DESCRIPTOR_HEAP_TYPE Type,
        uint32 InCapacity,
        std::string* ErrorMessage)
    {
        D3D12_DESCRIPTOR_HEAP_DESC Desc{};
        Desc.Type = Type;
        Desc.NumDescriptors = InCapacity;
        Desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        const HRESULT Result = Device->CreateDescriptorHeap(
            &Desc,
            IID_PPV_ARGS(Heap.GetAddressOf()));
        if (FAILED(Result))
        {
            SetErrorMessage(
                ErrorMessage,
                "CreateDescriptorHeap failed: " + FormatHResult(Result));
            return false;
        }

        Start = Heap->GetCPUDescriptorHandleForHeapStart();
        IncrementSize = Device->GetDescriptorHandleIncrementSize(Type);
        Capacity = InCapacity;
        Count = 0;
        return true;
    }

    bool D3D12DescriptorHeap::Allocate(D3D12_CPU_DESCRIPTOR_HANDLE& Handle)
    {
        if (Count >= Capacity)
        {
            return false;
        }

        Handle.ptr = Start.ptr + static_cast<SIZE_T>(Count) * IncrementSize;
        ++Count;
        return true;
    }

    void D3D12DescriptorHeap::Reset()
    {
        Count = 0;
    }

    ID3D12DescriptorHeap* D3D12DescriptorHeap::GetHeap() const
    {
        return Heap.Get();
    }

    ID3D12DescriptorHeap* D3D12MaterialBinding::GetSrvHeap() const
    {
        return SrvHeap.Get();
    }

    ID3D12DescriptorHeap* D3D12MaterialBinding::GetSamplerHeap() const
    {
        return SamplerHeap;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE D3D12MaterialBinding::GetSrvGpu() const
    {
        return SrvGpu;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE D3D12MaterialBinding::GetSamplerGpu() const
    {
        return SamplerGpu;
    }
}
