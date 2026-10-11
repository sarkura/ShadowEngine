#pragma once
#pragma pack_matrix(column_major)

#include "PerObject.slang"
#include "SceneConstants.slang"
#include "MaterialSurface.slang"

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
    uint4 MaterialPadding0;
    uint4 MaterialPadding1;
    ForwardDirectLight Direct;
    ForwardPointLight Point;
    ForwardSkyLight Sky;
};
