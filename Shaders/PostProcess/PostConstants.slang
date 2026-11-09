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
    float ExposureDelta;
    uint Source;
    uint Secondary;
    uint Motion;
    uint Depth;
    uint Sampler;
    uint ExposureMap;
    uint Pad2;
    uint Pad3;
    float ExposureMiddleGrey;
    float ExposureMinimum;
    float ExposureMaximum;
    float ExposureAdaptSpeed;
    float BloomLimit;
    float ReservedFloat;
    uint PostFlags;
    uint ReservedUint;
};

static const uint PostFlagAutoExposure = 1;
static const uint PostFlagBloom = 2;
static const uint PostFlagDither = 4;
