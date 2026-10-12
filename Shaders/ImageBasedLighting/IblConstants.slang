#pragma once
#pragma pack_matrix(column_major)

#include "../Common/DescriptorHeap.slang"

cbuffer IblConstants : register(b0)
{
    float2 DestSize;
    float Roughness;
    float Pad0;
    uint Source;
    uint Sampler;
    uint2 Pad;
};
