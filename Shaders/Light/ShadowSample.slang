#include "../Common/DescriptorHeap.slang"

float DirectShadowVisibility(float3 WorldPosition, float3 Normal)
{
    if (ShadowIndices.DirectMap == 0xffffffff || ShadowIndices.ComparisonSampler == 0xffffffff)
    {
        return 1.0;
    }

    const float3 UnitNormal = normalize(Normal);
    const float3 ToLight = normalize(-Direct.Direction.xyz);
    const float3 SamplePosition = WorldPosition + (UnitNormal + ToLight) * Shadow.Settings.y;
    const float4 Homogeneous = float4(SamplePosition, 1.0);
    const float4 Clip = float4(
        dot(Shadow.DirectRow0, Homogeneous),
        dot(Shadow.DirectRow1, Homogeneous),
        dot(Shadow.DirectRow2, Homogeneous),
        dot(Shadow.DirectRow3, Homogeneous));
    if (Clip.w <= 0.0)
    {
        return 1.0;
    }

    const float3 Ndc = Clip.xyz / Clip.w;
    const float2 Uv = float2(Ndc.x * 0.5 + 0.5, -Ndc.y * 0.5 + 0.5);
    if (Uv.x < 0.0 || Uv.y < 0.0 || Uv.x > 1.0 || Uv.y > 1.0 || Ndc.z < 0.0 || Ndc.z > 1.0)
    {
        return 1.0;
    }

    const Texture2D Map = LoadResourceDescriptor<Texture2D>(ShadowIndices.DirectMap);
    const SamplerComparisonState Comparison = LoadSamplerDescriptor<SamplerComparisonState>(ShadowIndices.ComparisonSampler);
    const float2 Texel = float2(Shadow.Settings.x, Shadow.Settings.x);
    const float Slope = 1.0 - saturate(dot(UnitNormal, ToLight));
    const float Reference = Ndc.z - (0.0004 + Slope * 0.002);
    float Sum = 0.0;
    [unroll]
    for (int Y = -1; Y <= 1; ++Y)
    {
        [unroll]
        for (int X = -1; X <= 1; ++X)
        {
            Sum += Map.SampleCmpLevelZero(Comparison, Uv + float2(X, Y) * Texel * 0.5, Reference);
        }
    }
    return Sum / 9.0;
}

float PointShadowVisibility(int LightIndex, float3 WorldPosition, float3 LightPosition, float Radius, float3 Normal)
{
    uint MapIndex = 0xffffffff;
    if (LightIndex == 0)
    {
        MapIndex = ShadowIndices.PointMap0;
    }
    else if (LightIndex == 1)
    {
        MapIndex = ShadowIndices.PointMap1;
    }
    else if (LightIndex == 2)
    {
        MapIndex = ShadowIndices.PointMap2;
    }
    else if (LightIndex == 3)
    {
        MapIndex = ShadowIndices.PointMap3;
    }

    if (MapIndex == 0xffffffff || ShadowIndices.ComparisonSampler == 0xffffffff)
    {
        return 1.0;
    }

    const float3 SamplePosition = WorldPosition + normalize(Normal) * 0.02;
    const float3 Offset = SamplePosition - LightPosition;
    const float Distance = length(Offset);
    if (Distance <= 0.0001)
    {
        return 1.0;
    }

    const float3 Direction = Offset / Distance;
    const float3 Up = abs(Direction.y) > 0.999 ? float3(0.0, 0.0, 1.0) : float3(0.0, 1.0, 0.0);
    const float3 Tangent = normalize(cross(Up, Direction));
    const float3 Bitangent = cross(Direction, Tangent);
    const float Texel = Shadow.Settings.z;
    const float Reference = saturate(Distance / max(Radius, 0.001)) - Shadow.Settings.w;
    const TextureCube Map = LoadResourceDescriptor<TextureCube>(MapIndex);
    const SamplerComparisonState Comparison = LoadSamplerDescriptor<SamplerComparisonState>(ShadowIndices.ComparisonSampler);
    float Sum = 0.0;
    [unroll]
    for (int Y = -1; Y <= 1; ++Y)
    {
        [unroll]
        for (int X = -1; X <= 1; ++X)
        {
            const float3 SampleDirection = Direction + (Tangent * float(X) + Bitangent * float(Y)) * Texel;
            Sum += Map.SampleCmpLevelZero(Comparison, SampleDirection, Reference);
        }
    }
    return Sum / 9.0;
}
