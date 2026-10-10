#pragma once
#pragma pack_matrix(column_major)

#include "PerObject.slang"
#include "SceneConstants.slang"

struct ForwardSurfaceData
{
    float4 BaseColor;
    float4 SpecularColor;
    float4 Roughness;
};

struct MaterialDescriptorIndices
{
    uint BaseColor;
    uint Roughness;
    uint Normal;
    uint Sampler;
};

struct ForwardDirectLight
{
    float4 Direction;
    float4 Color;
};

struct ForwardPointLight
{
    float4 Position;
    float4 Color;
};

struct ForwardSkyLight
{
    float4 Color;
};

cbuffer SceneConstants : register(b0)
{
    PerObjectData PerObject;
    ForwardSurfaceData Surface;
    MaterialDescriptorIndices Descriptors;
    ForwardDirectLight Direct;
    ForwardPointLight Point;
    ForwardSkyLight Sky;
};
