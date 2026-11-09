#pragma pack_matrix(column_major)

#include "../Common/DescriptorHeap.slang"

cbuffer ImGuiConstants : register(b0)
{
    float4x4 Projection;
    uint Texture;
    uint Sampler;
    uint2 Pad;
};

struct VertexInput
{
    float2 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
    float4 Color : COLOR0;
};

struct VertexOutput
{
    float4 Position : SV_Position;
    float4 Color : COLOR0;
    float2 TexCoord : TEXCOORD0;
};

[shader("vertex")]
VertexOutput VertexMain(VertexInput Input)
{
    VertexOutput Output;
    Output.Position = mul(Projection, float4(Input.Position, 0.0, 1.0));
    Output.Color = Input.Color;
    Output.TexCoord = Input.TexCoord;
    return Output;
}

[shader("fragment")]
float4 PixelMain(VertexOutput Input) : SV_Target
{
    const Texture2D Image = LoadResourceDescriptor<Texture2D>(Texture);
    const SamplerState ImageSampler = LoadSamplerDescriptor<SamplerState>(Sampler);
    return Input.Color * Image.Sample(ImageSampler, Input.TexCoord);
}
