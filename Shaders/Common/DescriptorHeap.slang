#pragma once

__generic<T>
T LoadResourceDescriptor(uint Index)
{
    __intrinsic_asm "($TR)(ResourceDescriptorHeap[$0])";
}

__generic<T>
T LoadSamplerDescriptor(uint Index)
{
    __intrinsic_asm "($TR)(SamplerDescriptorHeap[$0])";
}
