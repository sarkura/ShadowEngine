#include "RHI/Direct3D12/Public/D3D12Sampler.h"

namespace ShadowEngine
{
    D3D12Sampler::D3D12Sampler(uint32 InIndex)
        : Index(InIndex)
    {
    }

    uint32 D3D12Sampler::GetDescriptorIndex() const
    {
        return Index;
    }
}
