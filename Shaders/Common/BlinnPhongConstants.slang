#pragma once
#pragma pack_matrix(column_major)

#include "PerObject.slang"
#include "SceneConstants.slang"
#include "MaterialSurface.slang"

cbuffer SceneConstants : register(b0)
{
    PerObjectData PerObject;
    MaterialSurfaceData Surface;
    uint4 MaterialPadding;
};
