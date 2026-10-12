#pragma once

#include "../Common/DescriptorHeap.slang"

static const float IblPi = 3.14159265;

float RadicalInverseVdC(uint Bits)
{
    Bits = (Bits << 16u) | (Bits >> 16u);
    Bits = ((Bits & 0x55555555u) << 1u) | ((Bits & 0xAAAAAAAAu) >> 1u);
    Bits = ((Bits & 0x33333333u) << 2u) | ((Bits & 0xCCCCCCCCu) >> 2u);
    Bits = ((Bits & 0x0F0F0F0Fu) << 4u) | ((Bits & 0xF0F0F0F0u) >> 4u);
    Bits = ((Bits & 0x00FF00FFu) << 8u) | ((Bits & 0xFF00FF00u) >> 8u);
    return float(Bits) * 2.3283064365386963e-10;
}

float2 Hammersley(uint Index, uint Count)
{
    return float2(float(Index) / float(Count), RadicalInverseVdC(Index));
}

float3 EquirectToDirection(float2 Uv)
{
    const float Phi = (Uv.x - 0.5) * 6.283185307;
    const float Theta = Uv.y * IblPi;
    const float SinTheta = sin(Theta);
    return float3(cos(Phi) * SinTheta, cos(Theta), sin(Phi) * SinTheta);
}

float3 SampleEquirect(Texture2D Texture, SamplerState Sampler, float3 Direction)
{
    const float2 Uv = float2(
        atan2(Direction.z, Direction.x) * 0.159154943 + 0.5,
        acos(clamp(Direction.y, -1.0, 1.0)) * 0.318309886);
    return Texture.SampleLevel(Sampler, Uv, 0).rgb;
}

float3 SampleCube(TextureCube Texture, SamplerState Sampler, float3 Direction)
{
    return Texture.SampleLevel(Sampler, Direction, 0).rgb;
}

float3 ClampEnvironmentRadiance(float3 Radiance)
{
    const float Luminance = max(dot(Radiance, float3(0.2126, 0.7152, 0.0722)), 0.0001);
    return Radiance * min(16.0 / Luminance, 1.0);
}

float DistributionGGX(float NdotH, float Alpha)
{
    const float A2 = Alpha * Alpha;
    const float Denominator = NdotH * NdotH * (A2 - 1.0) + 1.0;
    return A2 / max(IblPi * Denominator * Denominator, 0.000001);
}

float VisibilitySmithGGX(float NdotV, float NdotL, float Alpha)
{
    const float A2 = Alpha * Alpha;
    const float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - A2) + A2);
    const float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - A2) + A2);
    return 0.5 / max(GGXV + GGXL, 0.000001);
}

float3 ImportanceSampleGGX(float2 Xi, float3 Normal, float Roughness)
{
    const float Alpha = Roughness * Roughness;
    const float A2 = Alpha * Alpha;
    const float Phi = 6.283185307 * Xi.x;
    const float CosTheta = sqrt((1.0 - Xi.y) / (1.0 + (A2 - 1.0) * Xi.y));
    const float SinTheta = sqrt(max(1.0 - CosTheta * CosTheta, 0.0));
    const float3 HalfVector = float3(SinTheta * cos(Phi), SinTheta * sin(Phi), CosTheta);
    const float3 Up = abs(Normal.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    const float3 Tangent = normalize(cross(Up, Normal));
    const float3 Bitangent = cross(Normal, Tangent);
    return normalize(Tangent * HalfVector.x + Bitangent * HalfVector.y + Normal * HalfVector.z);
}

float2 IntegrateBrdf(float NdotV, float Roughness)
{
    const float3 View = float3(sqrt(max(1.0 - NdotV * NdotV, 0.0)), 0.0, NdotV);
    const float3 Normal = float3(0.0, 0.0, 1.0);
    const float Alpha = max(Roughness * Roughness, 0.0001);
    float Scale = 0.0;
    float Bias = 0.0;
    const uint SampleCount = 128;
    [loop]
    for (uint Index = 0; Index < SampleCount; ++Index)
    {
        const float2 Xi = Hammersley(Index, SampleCount);
        const float3 HalfVector = ImportanceSampleGGX(Xi, Normal, Roughness);
        const float3 ToLight = normalize(2.0 * dot(View, HalfVector) * HalfVector - View);
        const float NdotL = saturate(ToLight.z);
        const float NdotH = saturate(HalfVector.z);
        const float VdotH = saturate(dot(View, HalfVector));
        if (NdotL > 0.0)
        {
            const float Visibility = VisibilitySmithGGX(NdotV, NdotL, Alpha);
            const float Geometry = 4.0 * NdotL * Visibility * VdotH / max(NdotH, 0.0001);
            const float Fresnel = pow(1.0 - VdotH, 5.0);
            Scale += (1.0 - Fresnel) * Geometry;
            Bias += Fresnel * Geometry;
        }
    }

    return float2(Scale, Bias) / float(SampleCount);
}

float3 SamplePrefiltered(
    Texture2D Level0,
    Texture2D Level1,
    Texture2D Level2,
    Texture2D Level3,
    SamplerState Sampler,
    float3 Direction,
    float Roughness)
{
    const float Scaled = saturate(Roughness) * 3.0;
    const uint Low = (uint)Scaled;
    const uint High = min(Low + 1, 3);
    const float Blend = Scaled - float(Low);
    float3 LowColor = SampleEquirect(Level0, Sampler, Direction);
    float3 HighColor = LowColor;
    if (Low == 1)
    {
        LowColor = SampleEquirect(Level1, Sampler, Direction);
    }
    else if (Low == 2)
    {
        LowColor = SampleEquirect(Level2, Sampler, Direction);
    }
    else if (Low >= 3)
    {
        LowColor = SampleEquirect(Level3, Sampler, Direction);
    }

    if (High == 1)
    {
        HighColor = SampleEquirect(Level1, Sampler, Direction);
    }
    else if (High == 2)
    {
        HighColor = SampleEquirect(Level2, Sampler, Direction);
    }
    else if (High >= 3)
    {
        HighColor = SampleEquirect(Level3, Sampler, Direction);
    }

    return lerp(LowColor, HighColor, Blend);
}

float3 EvaluateIBL(
    float3 Albedo,
    float Metallic,
    float Roughness,
    float Occlusion,
    float3 WorldNormal,
    float3 ViewDirection,
    uint Irradiance,
    uint Prefiltered0,
    uint Prefiltered1,
    uint Prefiltered2,
    uint Prefiltered3,
    uint BrdfLut,
    uint EnvironmentSampler,
    uint BrdfSampler)
{
    const float3 Normal = normalize(WorldNormal);
    const float3 View = normalize(ViewDirection);
    const float NdotV = max(dot(Normal, View), 0.001);
    const float Metal = saturate(Metallic);
    const float Rough = saturate(Roughness);
    const float3 ReflectDirection = reflect(-View, Normal);
    const float3 F0 = lerp(float3(0.04, 0.04, 0.04), Albedo, Metal);
    const Texture2D IrradianceMap = LoadResourceDescriptor<Texture2D>(Irradiance);
    const SamplerState WrapSampler = LoadSamplerDescriptor<SamplerState>(EnvironmentSampler);
    const float3 Diffuse = SampleEquirect(IrradianceMap, WrapSampler, Normal) * Albedo * (1.0 - Metal) * Occlusion;
    const float3 Prefiltered = SamplePrefiltered(
        LoadResourceDescriptor<Texture2D>(Prefiltered0),
        LoadResourceDescriptor<Texture2D>(Prefiltered1),
        LoadResourceDescriptor<Texture2D>(Prefiltered2),
        LoadResourceDescriptor<Texture2D>(Prefiltered3),
        WrapSampler,
        ReflectDirection,
        Rough);
    const Texture2D Brdf = LoadResourceDescriptor<Texture2D>(BrdfLut);
    const SamplerState ClampSampler = LoadSamplerDescriptor<SamplerState>(BrdfSampler);
    const float2 ScaleBias = Brdf.SampleLevel(ClampSampler, float2(NdotV, Rough), 0).rg;
    const float3 Specular = Prefiltered * (F0 * ScaleBias.x + ScaleBias.y);
    return Diffuse + Specular;
}
