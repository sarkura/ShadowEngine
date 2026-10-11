#pragma once
#pragma pack_matrix(column_major)

#include "PerObject.slang"
#include "SceneConstants.slang"
#include "MaterialSurface.slang"

#ifndef MATERIAL_DESCRIPTOR_INDICES_DEFINED
#define MATERIAL_DESCRIPTOR_INDICES_DEFINED
struct MaterialDescriptorIndices
{
    uint BaseColorTexture;
    uint RoughnessTexture;
    uint MetallicTexture;
    uint NormalTexture;
    uint OcclusionTexture;
    uint Sampler;
    uint Padding0;
    uint Padding1;
};
#endif

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
    MaterialSurfaceData Surface;
    MaterialDescriptorIndices Descriptors;
    ForwardDirectLight Direct;
    ForwardPointLight Point;
    ForwardSkyLight Sky;
};
