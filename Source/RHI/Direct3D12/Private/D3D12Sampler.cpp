#include "RHI/Direct3D12/Public/D3D12Sampler.h"

#include "Framework/Common/Public/Log.h"

namespace ShadowEngine
{
    std::unique_ptr<D3D12Sampler> D3D12Sampler::Create(ID3D12Device* Device, std::string* ErrorMessage)
    {
        D3D12_DESCRIPTOR_HEAP_DESC HeapDesc{};
        HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        HeapDesc.NumDescriptors = 1;
        HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        auto Sampler = std::unique_ptr<D3D12Sampler>(new D3D12Sampler());
        const HRESULT HeapResult = Device->CreateDescriptorHeap(
            &HeapDesc,
            IID_PPV_ARGS(Sampler->Heap.GetAddressOf()));
        if (FAILED(HeapResult))
        {
            SetErrorMessage(ErrorMessage, "CreateDescriptorHeap failed for the sampler: " + FormatHResult(HeapResult));
            return nullptr;
        }

        D3D12_SAMPLER_DESC Desc{};
        Desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        Desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        Desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        Desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        Desc.MipLODBias = 0.0F;
        Desc.MaxAnisotropy = 1;
        Desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        Desc.MinLOD = 0.0F;
        Desc.MaxLOD = D3D12_FLOAT32_MAX;

        const D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle = Sampler->Heap->GetCPUDescriptorHandleForHeapStart();
        Device->CreateSampler(&Desc, CpuHandle);
        Sampler->GpuHandle = Sampler->Heap->GetGPUDescriptorHandleForHeapStart();
        return Sampler;
    }

    ID3D12DescriptorHeap* D3D12Sampler::GetHeap() const
    {
        return Heap.Get();
    }

    D3D12_GPU_DESCRIPTOR_HANDLE D3D12Sampler::GetGPUHandle() const
    {
        return GpuHandle;
    }
}
