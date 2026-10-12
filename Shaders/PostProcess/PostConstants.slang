#pragma once
#pragma pack_matrix(column_major)

#include "../Common/DescriptorHeap.slang"

cbuffer PostConstants : register(b0)
{
    float4x4 InverseViewProjection;
    float4x4 PreviousViewProjection;
    float4 CameraPosition;
    float2 DestSize;
    float2 SourceTexel;
    float Threshold;
    float Strength;
    float CurrentWeight;
    float Pad0;
    uint Source;
    uint Secondary;
    uint Motion;
    uint Depth;
    uint Sampler;
    uint Pad1;
    uint Pad2;
    uint Pad3;
};
