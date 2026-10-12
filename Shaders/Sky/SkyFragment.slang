#pragma pack_matrix(column_major)

#include "../Common/DescriptorHeap.slang"

cbuffer SkyConstants : register(b0)
{
    float4x4 InverseViewProjection;
    float4 CameraPosition;
    float4 Viewport;
    uint Environment;
    uint Sampler;
    uint2 Pad;
};

[shader("fragment")]
float4 PixelMain(float4 Position : SV_Position) : SV_Target
{
    const float2 Ndc = float2(Position.x / Viewport.x * 2.0 - 1.0, 1.0 - Position.y / Viewport.y * 2.0);
    const float4 World = mul(InverseViewProjection, float4(Ndc, 1.0, 1.0));
    const float3 Direction = normalize(World.xyz / World.w - CameraPosition.xyz);
    const TextureCube EnvironmentMap = LoadResourceDescriptor<TextureCube>(Environment);
    const SamplerState EnvironmentSampler = LoadSamplerDescriptor<SamplerState>(Sampler);
    float3 Color = EnvironmentMap.SampleLevel(EnvironmentSampler, Direction, 0).rgb;
    return float4(Color, 1.0);
}
