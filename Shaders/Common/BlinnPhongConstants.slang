#pragma once
#pragma pack_matrix(column_major)

#include "PerObject.slang"
#include "SceneConstants.slang"
#include "BlinnPhongSurface.slang"

cbuffer SceneConstants : register(b0)
{
    PerObjectData PerObject;
    BlinnPhongSurfaceData BlinnPhongSurface;
};
