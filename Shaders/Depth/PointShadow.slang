#include "ShadowConstants.slang"

struct PointShadowVarying
{
    float4 Position : SV_Position;
    float3 WorldPosition : TEXCOORD0;
};

[shader("vertex")]
PointShadowVarying VertexMain(ShadowVertexInput Input)
{
    PointShadowVarying Output;
    const float4 WorldPosition = mul(World, float4(Input.Position, 1.0));
    Output.WorldPosition = WorldPosition.xyz;
    Output.Position = mul(ViewProjection, WorldPosition);
    return Output;
}

[shader("fragment")]
float PixelMain(PointShadowVarying Input) : SV_Depth
{
    const float Radius = max(LightPosition.w, 0.001);
    return saturate(length(Input.WorldPosition - LightPosition.xyz) / Radius);
}
