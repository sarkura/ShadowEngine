#include "../Common/BlinnPhongConstants.slang"

[shader("vertex")]
VertexOutput VertexMain(VertexInput Input)
{
    VertexOutput Output;
    const float4 WorldPosition = mul(PerObject.World, float4(Input.Position, 1.0));
    Output.Position = mul(PerObject.ViewProjection, WorldPosition);
    Output.WorldPosition = WorldPosition.xyz;
    Output.WorldNormal = mul((float3x3)PerObject.NormalMatrix, Input.Normal);
    Output.Color = Input.Color;
    return Output;
}

#include "BlinnPhongFragment.slang"
