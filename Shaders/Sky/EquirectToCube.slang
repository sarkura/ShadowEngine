#pragma pack_matrix(column_major)

#include "../Common/DescriptorHeap.slang"
#include "../Light/ImageBasedLighting.slang"

cbuffer CubeConstants : register(b0)
{
    float2 DestSize;
    uint Face;
    uint Pad0;
    uint Source;
    uint Sampler;
    uint2 Pad1;
};

float3 CubemapFaceDirection(uint FaceIndex, float2 Pixel, float2 Size)
{
    const float2 St = (Pixel + 0.5) / Size * 2.0 - 1.0;
    const float Sc = St.x;
    const float Tc = St.y;
    if (FaceIndex == 0)
    {
        return normalize(float3(1.0, -Tc, -Sc));
    }
    if (FaceIndex == 1)
    {
        return normalize(float3(-1.0, -Tc, Sc));
    }
    if (FaceIndex == 2)
    {
        return normalize(float3(Sc, 1.0, Tc));
    }
    if (FaceIndex == 3)
    {
        return normalize(float3(Sc, -1.0, -Tc));
    }
    if (FaceIndex == 4)
    {
        return normalize(float3(Sc, -Tc, 1.0));
    }
    return normalize(float3(-Sc, -Tc, -1.0));
}

[shader("fragment")]
float4 PixelMain(float4 Position : SV_Position) : SV_Target
{
    const float3 Direction = CubemapFaceDirection(Face, Position.xy, DestSize);
    const Texture2D EnvironmentMap = LoadResourceDescriptor<Texture2D>(Source);
    const SamplerState EnvironmentSampler = LoadSamplerDescriptor<SamplerState>(Sampler);
    return float4(SampleEquirect(EnvironmentMap, EnvironmentSampler, Direction), 1.0);
}
