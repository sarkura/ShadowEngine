#pragma pack_matrix(column_major)

#include "../Common/PerObject.slang"

struct VelocityInput
{
    float3 Position : POSITION;
};

struct VelocityOutput
{
    float4 Position : SV_Position;
    float4 CurrentClip : CURRENT_CLIP;
    float4 PreviousClip : PREVIOUS_CLIP;
};

cbuffer VelocityConstants : register(b0)
{
    PerObjectData PerObject;
};

[shader("vertex")]
VelocityOutput VertexMain(VelocityInput Input)
{
    VelocityOutput Output;
    const float4 Position = float4(Input.Position, 1.0);
    Output.CurrentClip = mul(PerObject.ViewProjection, mul(PerObject.World, Position));
    Output.PreviousClip = mul(PerObject.PreviousViewProjection, mul(PerObject.PreviousWorld, Position));
    Output.Position = Output.CurrentClip;
    return Output;
}

[shader("fragment")]
float4 PixelMain(VelocityOutput Input) : SV_Target
{
    const float2 CurrentUv = Input.CurrentClip.xy / Input.CurrentClip.w * float2(0.5, -0.5) + 0.5;
    const float2 PreviousUv = Input.PreviousClip.xy / Input.PreviousClip.w * float2(0.5, -0.5) + 0.5;
    return float4(PreviousUv - CurrentUv, 0.0, 0.0);
}
