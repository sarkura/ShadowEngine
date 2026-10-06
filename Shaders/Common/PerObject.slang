#pragma once
#pragma pack_matrix(column_major)

struct PerObjectData
{
    float4x4 ViewProjection;
    float4x4 World;
    float4x4 NormalMatrix;
    float4 CameraPosition;
    float4 LightDirection;
    float4 LightColor;
};
