#pragma pack_matrix(column_major)

struct ShadowVertexInput
{
    float3 Position : POSITION;
};

cbuffer SceneConstants : register(b0)
{
    float4x4 ViewProjection;
    float4x4 World;
    float4 LightPosition;
};
