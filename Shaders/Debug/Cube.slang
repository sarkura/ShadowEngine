#pragma pack_matrix(column_major)

struct VertexInput
{
    float3 Position : POSITION;
    float3 Color : COLOR0;
};

struct VertexOutput
{
    float4 Position : SV_Position;
    float3 Color : COLOR0;
};

cbuffer SceneConstants : register(b0)
{
    float4x4 ViewProjection;
    float4x4 World;
};

[shader("vertex")]
VertexOutput VertexMain(VertexInput Input)
{
    VertexOutput Output;
    Output.Position = mul(ViewProjection, mul(World, float4(Input.Position, 1.0)));
    Output.Color = Input.Color;
    return Output;
}

[shader("fragment")]
float4 PixelMain(VertexOutput Input) : SV_Target
{
    return float4(Input.Color, 1.0);
}
