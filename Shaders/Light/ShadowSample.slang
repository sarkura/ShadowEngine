#include "../Common/DescriptorHeap.slang"

#ifndef SHADOW_CONSTANTS_DEFINED
#define SHADOW_CONSTANTS_DEFINED
#include "../Common/ForwardLights.slang"
#include "../Common/ForwardShadow.slang"
cbuffer ShadowSampleConstants
{
    ForwardDirectLight Direct;
    ForwardShadow Shadow;
    ForwardShadowIndices ShadowIndices;
};
#endif

float SampleVirtualPage(
    float2 Uv,
    float Level,
    float Reference,
    Texture2D Atlas,
    Texture2D Pages,
    SamplerState PointSampler)
{
    const float PagesPerAxis = Shadow.VirtualInfo.x;
    const float PageSize = Shadow.VirtualInfo.y;
    const float2 VirtualTexel = Uv * PagesPerAxis * PageSize;
    const int PageSizeInt = int(PageSize);
    const int2 Page = int2(floor(VirtualTexel / PageSize));
    if (Page.x < 0 || Page.y < 0 || Page.x >= int(PagesPerAxis) || Page.y >= int(PagesPerAxis))
    {
        return 1.0;
    }

    const float2 PageUv = frac(VirtualTexel / PageSize);
    const float TableHeight = PagesPerAxis * 4.0;
    const float2 TableUv = float2(
        (float(Page.x) + 0.5) / PagesPerAxis,
        (Level * PagesPerAxis + float(Page.y) + 0.5) / TableHeight);
    const float4 Entry = Pages.SampleLevel(PointSampler, TableUv, 0);
    if (Entry.b < 0.5)
    {
        return 1.0;
    }

    const int2 Physical = int2(floor(Entry.r * 255.0 + 0.5), floor(Entry.g * 255.0 + 0.5));
    const int2 Local = clamp(int2(floor(PageUv * PageSize)), int2(0, 0), int2(PageSizeInt - 1, PageSizeInt - 1));
    const float Depth = Atlas.Load(int3(Physical * PageSizeInt + Local, 0)).r;
    return Reference <= Depth ? 1.0 : 0.0;
}

bool EvaluateClipLevel(
    float4 Row0,
    float4 Row1,
    float4 Row2,
    float4 Row3,
    float Level,
    float3 SamplePosition,
    float Bias,
    Texture2D Atlas,
    Texture2D Pages,
    SamplerState PointSampler,
    out float Visibility)
{
    Visibility = 1.0;
    const float4 Homogeneous = float4(SamplePosition, 1.0);
    const float4 Clip = float4(dot(Row0, Homogeneous), dot(Row1, Homogeneous), dot(Row2, Homogeneous), dot(Row3, Homogeneous));
    if (Clip.w <= 0.0)
    {
        return false;
    }

    const float3 Ndc = Clip.xyz / Clip.w;
    const float2 Uv = float2(Ndc.x * 0.5 + 0.5, -Ndc.y * 0.5 + 0.5);
    if (Uv.x < 0.0 || Uv.y < 0.0 || Uv.x > 1.0 || Uv.y > 1.0 || Ndc.z < 0.0 || Ndc.z > 1.0)
    {
        return false;
    }

    const float PagesPerAxis = Shadow.VirtualInfo.x;
    const int2 Page = int2(floor(Uv * PagesPerAxis));
    const float TableHeight = PagesPerAxis * 4.0;
    const float2 TableUv = float2(
        (float(Page.x) + 0.5) / PagesPerAxis,
        (Level * PagesPerAxis + float(Page.y) + 0.5) / TableHeight);
    if (Pages.SampleLevel(PointSampler, TableUv, 0).b < 0.5)
    {
        return false;
    }

    const float Reference = Ndc.z - Bias;
    const float Texel = 1.0 / (PagesPerAxis * Shadow.VirtualInfo.y);
    float Sum = 0.0;
    [unroll]
    for (int Y = -1; Y <= 1; ++Y)
    {
        [unroll]
        for (int X = -1; X <= 1; ++X)
        {
            Sum += SampleVirtualPage(Uv + float2(X, Y) * Texel * 0.5, Level, Reference, Atlas, Pages, PointSampler);
        }
    }
    Visibility = Sum / 9.0;
    return true;
}

float VirtualDirectVisibility(float3 WorldPosition, float3 Normal)
{
    const float3 UnitNormal = normalize(Normal);
    const float3 ToLight = normalize(-Direct.Direction.xyz);
    const float3 SamplePosition = WorldPosition + (UnitNormal + ToLight) * Shadow.Settings.y;
    const Texture2D Atlas = LoadResourceDescriptor<Texture2D>(ShadowIndices.DirectMap);
    const Texture2D Pages = LoadResourceDescriptor<Texture2D>(ShadowIndices.PageTable);
    const SamplerState PointSampler = LoadSamplerDescriptor<SamplerState>(ShadowIndices.PageSampler);
    const float Slope = 1.0 - saturate(dot(UnitNormal, ToLight));
    const float Bias = 0.0004 + Slope * 0.002;
    float Visibility = 1.0;
    if (EvaluateClipLevel(Shadow.Level0Row0, Shadow.Level0Row1, Shadow.Level0Row2, Shadow.Level0Row3, 0.0, SamplePosition, Bias, Atlas, Pages, PointSampler, Visibility) ||
        EvaluateClipLevel(Shadow.Level1Row0, Shadow.Level1Row1, Shadow.Level1Row2, Shadow.Level1Row3, 1.0, SamplePosition, Bias, Atlas, Pages, PointSampler, Visibility) ||
        EvaluateClipLevel(Shadow.Level2Row0, Shadow.Level2Row1, Shadow.Level2Row2, Shadow.Level2Row3, 2.0, SamplePosition, Bias, Atlas, Pages, PointSampler, Visibility) ||
        EvaluateClipLevel(Shadow.Level3Row0, Shadow.Level3Row1, Shadow.Level3Row2, Shadow.Level3Row3, 3.0, SamplePosition, Bias, Atlas, Pages, PointSampler, Visibility))
    {
        return Visibility;
    }
    return 1.0;
}

float DirectShadowVisibility(float3 WorldPosition, float3 Normal)
{
    if (ShadowIndices.PageTable != 0xffffffff && ShadowIndices.PageSampler != 0xffffffff &&
        ShadowIndices.DirectMap != 0xffffffff && ShadowIndices.ComparisonSampler != 0xffffffff)
    {
        return VirtualDirectVisibility(WorldPosition, Normal);
    }

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
