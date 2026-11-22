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
    const float TableHeight = PagesPerAxis * Shadow.Settings.x;
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
    const float TableHeight = PagesPerAxis * Shadow.Settings.x;
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
    if (ShadowIndices.DirectMap == 0xffffffff || ShadowIndices.PageTable == 0xffffffff ||
        ShadowIndices.PageSampler == 0xffffffff)
    {
        return 1.0;
    }

    return VirtualDirectVisibility(WorldPosition, Normal);
}

void PointFaceAxes(int Face, out float3 Forward, out float3 Up)
{
    if (Face == 0)
    {
        Forward = float3(1.0, 0.0, 0.0);
        Up = float3(0.0, 1.0, 0.0);
    }
    else if (Face == 1)
    {
        Forward = float3(-1.0, 0.0, 0.0);
        Up = float3(0.0, 1.0, 0.0);
    }
    else if (Face == 2)
    {
        Forward = float3(0.0, 1.0, 0.0);
        Up = float3(0.0, 0.0, -1.0);
    }
    else if (Face == 3)
    {
        Forward = float3(0.0, -1.0, 0.0);
        Up = float3(0.0, 0.0, 1.0);
    }
    else if (Face == 4)
    {
        Forward = float3(0.0, 0.0, 1.0);
        Up = float3(0.0, 1.0, 0.0);
    }
    else
    {
        Forward = float3(0.0, 0.0, -1.0);
        Up = float3(0.0, 1.0, 0.0);
    }
}

int PointFaceIndex(float3 Direction)
{
    const float3 Absolute = abs(Direction);
    if (Absolute.x >= Absolute.y && Absolute.x >= Absolute.z)
    {
        return Direction.x >= 0.0 ? 0 : 1;
    }
    if (Absolute.y >= Absolute.z)
    {
        return Direction.y >= 0.0 ? 2 : 3;
    }
    return Direction.z >= 0.0 ? 4 : 5;
}

void PerspectiveFaceRows(
    float3 Eye,
    float FarPlane,
    float3 Forward,
    float3 Up,
    float TanHalf,
    out float4 Row0,
    out float4 Row1,
    out float4 Row2,
    out float4 Row3)
{
    const float3 F = normalize(Forward);
    const float3 S = normalize(cross(F, Up));
    const float3 U = cross(S, F);
    const float Scale = 1.0 / TanHalf;
    const float4 View0 = float4(S, -dot(S, Eye));
    const float4 View1 = float4(U, -dot(U, Eye));
    const float4 View2 = float4(-F, dot(F, Eye));
    const float NearPlane = 0.05;
    const float C = FarPlane / (NearPlane - FarPlane);
    const float D = -(FarPlane * NearPlane) / (FarPlane - NearPlane);
    Row0 = View0 * Scale;
    Row1 = View1 * Scale;
    Row2 = C * View2 + float4(0.0, 0.0, 0.0, D);
    Row3 = -View2;
}

void PointFaceRows(float3 Eye, float FarPlane, int Face, out float4 Row0, out float4 Row1, out float4 Row2, out float4 Row3)
{
    float3 Forward;
    float3 Up;
    PointFaceAxes(Face, Forward, Up);
    PerspectiveFaceRows(Eye, FarPlane, Forward, Up, 1.0, Row0, Row1, Row2, Row3);
}

float PointShadowVisibility(int LightIndex, float3 WorldPosition, float3 LightPosition, float Radius, float3 Normal)
{
    if (ShadowIndices.DirectMap == 0xffffffff || ShadowIndices.PageTable == 0xffffffff ||
        ShadowIndices.PageSampler == 0xffffffff)
    {
        return 1.0;
    }

    const float3 UnitNormal = normalize(Normal);
    const float3 ToLight = normalize(LightPosition - WorldPosition);
    const float3 SamplePosition = WorldPosition + (UnitNormal + ToLight) * Shadow.Settings.y;
    const float3 FromLight = SamplePosition - LightPosition;
    const int Face = PointFaceIndex(FromLight);
    const float FarPlane = max(Radius, 0.15);
    float4 Row0;
    float4 Row1;
    float4 Row2;
    float4 Row3;
    PointFaceRows(LightPosition, FarPlane, Face, Row0, Row1, Row2, Row3);
    const float Level = 4.0 + float(LightIndex) * 6.0 + float(Face);
    const float Slope = 1.0 - saturate(dot(UnitNormal, ToLight));
    const float Bias = 0.0004 + Slope * 0.002;
    const Texture2D Atlas = LoadResourceDescriptor<Texture2D>(ShadowIndices.DirectMap);
    const Texture2D Pages = LoadResourceDescriptor<Texture2D>(ShadowIndices.PageTable);
    const SamplerState PointSampler = LoadSamplerDescriptor<SamplerState>(ShadowIndices.PageSampler);
    float Visibility = 1.0;
    if (EvaluateClipLevel(Row0, Row1, Row2, Row3, Level, SamplePosition, Bias, Atlas, Pages, PointSampler, Visibility))
    {
        return Visibility;
    }
    return 1.0;
}

float AreaShadowVisibility(
    int LightIndex,
    float3 WorldPosition,
    float3 LightPosition,
    float3 Forward,
    float3 Up,
    float FarPlane,
    float3 Normal)
{
    if (ShadowIndices.DirectMap == 0xffffffff || ShadowIndices.PageTable == 0xffffffff ||
        ShadowIndices.PageSampler == 0xffffffff)
    {
        return 1.0;
    }

    const float3 UnitNormal = normalize(Normal);
    const float3 ToLight = normalize(LightPosition - WorldPosition);
    const float3 SamplePosition = WorldPosition + (UnitNormal + ToLight) * Shadow.Settings.y;
    float4 Row0;
    float4 Row1;
    float4 Row2;
    float4 Row3;
    PerspectiveFaceRows(LightPosition, FarPlane, Forward, Up, tan(radians(60.0)), Row0, Row1, Row2, Row3);
    const float Level = 28.0 + float(LightIndex);
    const float Slope = 1.0 - saturate(dot(UnitNormal, ToLight));
    const float Bias = 0.0004 + Slope * 0.002;
    const Texture2D Atlas = LoadResourceDescriptor<Texture2D>(ShadowIndices.DirectMap);
    const Texture2D Pages = LoadResourceDescriptor<Texture2D>(ShadowIndices.PageTable);
    const SamplerState PointSampler = LoadSamplerDescriptor<SamplerState>(ShadowIndices.PageSampler);
    float Visibility = 1.0;
    if (EvaluateClipLevel(Row0, Row1, Row2, Row3, Level, SamplePosition, Bias, Atlas, Pages, PointSampler, Visibility))
    {
        return Visibility;
    }
    return 1.0;
}
