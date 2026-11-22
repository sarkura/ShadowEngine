#pragma once
#pragma pack_matrix(column_major)

#include "ForwardLights.slang"
#include "ImageBasedLightIndices.slang"
#include "ForwardShadow.slang"

#define SHADOW_CONSTANTS_DEFINED
cbuffer DeferredLightConstants : register(b0)
{
    float4x4 InverseViewProjection;
    float4 CameraPosition;
    float4 Viewport;
    ForwardDirectLight Direct;
    ForwardPointLight Point[ForwardPointLightCapacity];
    ForwardAreaLight Area[ForwardAreaLightCapacity];
    ForwardSkyLight Sky;
    ImageBasedLightIndices ImageBasedLight;
    ForwardShadow Shadow;
    ForwardShadowIndices ShadowIndices;
    uint GBufferA;
    uint GBufferB;
    uint GBufferC;
    uint Depth;
    uint Sampler;
    uint Pad0;
    uint Pad1;
    uint Pad2;
    uint Pad3;
};
