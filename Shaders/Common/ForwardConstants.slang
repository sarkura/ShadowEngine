#pragma once
#pragma pack_matrix(column_major)

#include "PerObject.slang"
#include "SceneConstants.slang"
#include "MaterialSurface.slang"
#include "ForwardLights.slang"
#include "ImageBasedLightIndices.slang"

cbuffer SceneConstants : register(b0)
{
    PerObjectData PerObject;
    MaterialSurfaceData Surface;
    uint4 MaterialPadding0;
    uint4 MaterialPadding1;
    ForwardDirectLight Direct;
    ForwardPointLight Point[ForwardPointLightCapacity];
    ForwardSkyLight Sky;
    ImageBasedLightIndices ImageBasedLight;
};
