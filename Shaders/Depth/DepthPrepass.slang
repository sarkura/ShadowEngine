#pragma pack_matrix(column_major)

#include "../Common/PerObject.slang"

struct DepthVertexInput
{
    float3 Position : POSITION;
};

cbuffer SceneConstants : register(b0)
{
    PerObjectData PerObject;
};

[shader("vertex")]
float4 VertexMain(DepthVertexInput Input) : SV_Position
{
    const float4 Position = float4(Input.Position, 1.0);
    return mul(PerObject.ViewProjection, mul(PerObject.World, Position));
}

[shader("fragment")]
float4 PixelMain() : SV_Target
{
    return float4(0.0, 0.0, 0.0, 0.0);
}
