#include "RHI/Direct3D12/Public/D3D12Descriptor.h"

#include "Framework/Common/Public/Log.h"
#include "RHI/Direct3D12/Public/D3D12Sampler.h"
#include "RHI/Direct3D12/Public/D3D12Texture.h"

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

    bool D3D12MaterialBinding::Initialize(
        ID3D12Device* Device,
        D3D12Texture& BaseColor,
        D3D12Texture& Roughness,
        D3D12Sampler& Sampler,
        std::string* ErrorMessage)
    {
        D3D12_DESCRIPTOR_HEAP_DESC HeapDesc{};
        HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        HeapDesc.NumDescriptors = 2;
        HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        const HRESULT Result = Device->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(SrvHeap.GetAddressOf()));
        if (FAILED(Result))
        {
            SetErrorMessage(ErrorMessage, "CreateDescriptorHeap failed for material textures: " + FormatHResult(Result));
            return false;
        }

        const UINT Increment = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle = SrvHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12Texture* Textures[] = {&BaseColor, &Roughness};
        for (D3D12Texture* Texture : Textures)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC View{};
            View.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            View.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            View.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            View.Texture2D.MostDetailedMip = 0;
            View.Texture2D.MipLevels = Texture->GetMipCount();
            Device->CreateShaderResourceView(Texture->GetResource(), &View, CpuHandle);
            CpuHandle.ptr += Increment;
        }

        SrvGpu = SrvHeap->GetGPUDescriptorHandleForHeapStart();
        SamplerHeap = Sampler.GetHeap();
        SamplerGpu = Sampler.GetGPUHandle();
        return SamplerHeap != nullptr;
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
