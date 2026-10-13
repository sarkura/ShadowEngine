#include "ForwardLightSample.slang"
#include "ImageBasedLighting.slang"
#include "ShadowSample.slang"

float3 ShadeGgx(
    float3 Albedo,
    float Metallic,
    float Roughness,
    float3 Normal,
    float3 ToView,
    float3 ToLight,
    float3 Radiance)
{
    const float NdotL = saturate(dot(Normal, ToLight));
    if (NdotL <= 0.0)
    {
        return float3(0.0, 0.0, 0.0);
    }

    const float3 HalfVector = normalize(ToLight + ToView);
    const float NdotV = max(dot(Normal, ToView), 0.001);
    const float NdotH = saturate(dot(Normal, HalfVector));
    const float VdotH = saturate(dot(ToView, HalfVector));
    const float Alpha = max(Roughness * Roughness, 0.0001);
    const float3 F0 = lerp(float3(0.04, 0.04, 0.04), Albedo, saturate(Metallic));
    const float3 Fresnel = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
    const float Distribution = DistributionGGX(NdotH, Alpha);
    const float Visibility = VisibilitySmithGGX(NdotV, NdotL, Alpha);
    const float3 Specular = Distribution * Visibility * Fresnel;
    const float3 Diffuse = (1.0 - Fresnel) * (1.0 - saturate(Metallic)) * Albedo / IblPi;
    return (Diffuse + Specular) * Radiance * NdotL;
}

float3 ShadeDirectional(ForwardMaterialSample Material, float3 Normal, float3 ToView, float3 WorldPosition)
{
    if (Direct.Color.a <= 0.0)
    {
        return float3(0.0, 0.0, 0.0);
    }

    const float3 ToLight = normalize(-Direct.Direction.xyz);
    if (dot(Normal, ToLight) <= 0.0)
    {
        return float3(0.0, 0.0, 0.0);
    }

    const float Visibility = DirectShadowVisibility(WorldPosition, Normal);
    const float3 Radiance = Direct.Color.rgb * Direct.Color.a * Visibility;
    return ShadeGgx(Material.Albedo, Material.Metallic, Material.Roughness, Normal, ToView, ToLight, Radiance);
}

float3 ShadePoint(ForwardPointLight Light, int LightIndex, ForwardMaterialSample Material, float3 Normal, float3 WorldPosition, float3 ToView)
{
    if (Light.Color.a <= 0.0)
    {
        return float3(0.0, 0.0, 0.0);
    }

    const float3 Offset = Light.Position.xyz - WorldPosition;
    const float Distance2 = dot(Offset, Offset);
    const float Radius = max(Light.Position.w, 0.001);
    const float Radius2 = Radius * Radius;
    if (Distance2 >= Radius2)
    {
        return float3(0.0, 0.0, 0.0);
    }

    const float Distance = sqrt(Distance2);
    const float3 ToLight = Offset / max(Distance, 0.001);
    if (dot(Normal, ToLight) <= 0.0)
    {
        return float3(0.0, 0.0, 0.0);
    }

    const float Falloff = Distance2 / Radius2;
    const float Window = saturate(1.0 - Falloff * Falloff);
    const float Attenuation = Window * Window / (Distance2 + 1.0);
    const float Visibility = PointShadowVisibility(LightIndex, WorldPosition, Light.Position.xyz, Radius, Normal);
    const float3 Radiance = Light.Color.rgb * Light.Color.a * Attenuation * Visibility;
    return ShadeGgx(Material.Albedo, Material.Metallic, Material.Roughness, Normal, ToView, ToLight, Radiance);
}

ForwardLightSample EvaluateLight(ForwardMaterialSample Material, VertexOutput Input)
{
    const float3 Normal = normalize(Material.Normal);
    const float3 ToView = normalize(PerObject.CameraPosition.xyz - Input.WorldPosition);
    ForwardLightSample Sample;
    Sample.Direct = ShadeDirectional(Material, Normal, ToView, Input.WorldPosition);
    Sample.Point = float3(0.0, 0.0, 0.0);
    for (int Index = 0; Index < ForwardPointLightCapacity; ++Index)
    {
        Sample.Point += ShadePoint(Point[Index], Index, Material, Normal, Input.WorldPosition, ToView);
    }
    Sample.Sky = EvaluateIBL(
        Material.Albedo,
        Material.Metallic,
        Material.Roughness,
        Material.Occlusion,
        Normal,
        ToView,
        ImageBasedLight.Irradiance,
        ImageBasedLight.Prefiltered0,
        ImageBasedLight.Prefiltered1,
        ImageBasedLight.Prefiltered2,
        ImageBasedLight.Prefiltered3,
        ImageBasedLight.BrdfLut,
        ImageBasedLight.EnvironmentSampler,
        ImageBasedLight.BrdfSampler) * Sky.Color.rgb * Sky.Color.a;
    return Sample;
}
