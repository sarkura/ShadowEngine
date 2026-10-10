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
    const float2 Equirect = float2(
        atan2(Direction.z, Direction.x) * 0.159154943 + 0.5,
        acos(clamp(Direction.y, -1.0, 1.0)) * 0.318309886);
    const Texture2D EnvironmentMap = LoadResourceDescriptor<Texture2D>(Environment);
    const SamplerState EnvironmentSampler = LoadSamplerDescriptor<SamplerState>(Sampler);
    float3 Color = EnvironmentMap.SampleLevel(EnvironmentSampler, Equirect, 0).rgb;
    Color = Color / (Color + 1.0);
    return float4(Color, 1.0);
}
