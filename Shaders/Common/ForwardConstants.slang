#pragma once
#pragma pack_matrix(column_major)

#include "PerObject.slang"
#include "SceneConstants.slang"
#include "MaterialSurface.slang"
#include "ForwardLights.slang"
#include "ImageBasedLightIndices.slang"
#include "ForwardShadow.slang"

#define SHADOW_CONSTANTS_DEFINED
cbuffer SceneConstants : register(b0)
{
    PerObjectData PerObject;
    MaterialSurfaceData Surface;
    uint4 MaterialPadding0;
    uint4 MaterialPadding1;
    ForwardDirectLight Direct;
    ForwardPointLight Point[ForwardPointLightCapacity];
    ForwardAreaLight Area[ForwardAreaLightCapacity];
    ForwardSkyLight Sky;
    ImageBasedLightIndices ImageBasedLight;
    ForwardShadow Shadow;
    ForwardShadowIndices ShadowIndices;
};
