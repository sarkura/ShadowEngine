#pragma once
#pragma pack_matrix(column_major)

#include "PerObject.slang"
#include "SceneConstants.slang"
#include "MaterialSurface.slang"
#include "ForwardLights.slang"
#include "ImageBasedLightIndices.slang"

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

cbuffer SceneConstants : register(b0)
{
    PerObjectData PerObject;
    MaterialSurfaceData Surface;
    MaterialDescriptorIndices Descriptors;
    ForwardDirectLight Direct;
    ForwardPointLight Point[ForwardPointLightCapacity];
    ForwardSkyLight Sky;
    ImageBasedLightIndices ImageBasedLight;
};
