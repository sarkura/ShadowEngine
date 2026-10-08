#pragma once
#pragma pack_matrix(column_major)

#include "PerObject.slang"
#include "SceneConstants.slang"
#include "BlinnPhongSurface.slang"

struct MaterialDescriptorIndices
{
    uint BaseColor;
    uint Roughness;
    uint Normal;
    uint Sampler;
};

cbuffer SceneConstants : register(b0)
{
    PerObjectData PerObject;
    BlinnPhongSurfaceData BlinnPhongSurface;
    MaterialDescriptorIndices Descriptors;
};
