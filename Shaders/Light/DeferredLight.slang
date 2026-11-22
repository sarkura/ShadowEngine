#include "../Common/DeferredLightConstants.slang"
#include "../Common/DescriptorHeap.slang"
#include "ImageBasedLighting.slang"
#include "ShadowSample.slang"
#include "ForwardLightSample.slang"

float3 ShadeDeferredGgx(
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

[shader("fragment")]
float4 PixelMain(float4 Position : SV_Position) : SV_Target
{
    const float2 Uv = Position.xy / Viewport.xy;
    const SamplerState Clamp = LoadSamplerDescriptor<SamplerState>(Sampler);
    const Texture2D DepthMap = LoadResourceDescriptor<Texture2D>(Depth);
    const float SceneDepth = DepthMap.SampleLevel(Clamp, Uv, 0).r;
    if (SceneDepth >= 1.0)
    {
        discard;
    }

    const float2 Ndc = float2(Uv.x * 2.0 - 1.0, 1.0 - Uv.y * 2.0);
    const float4 World = mul(InverseViewProjection, float4(Ndc, SceneDepth, 1.0));
    const float3 WorldPosition = World.xyz / World.w;
    const float4 AlbedoMetal = LoadResourceDescriptor<Texture2D>(GBufferA).SampleLevel(Clamp, Uv, 0);
    const float4 NormalSample = LoadResourceDescriptor<Texture2D>(GBufferB).SampleLevel(Clamp, Uv, 0);
    const float4 RoughnessOcclusion = LoadResourceDescriptor<Texture2D>(GBufferC).SampleLevel(Clamp, Uv, 0);
    const float3 Albedo = AlbedoMetal.rgb;
    const float Metallic = AlbedoMetal.a;
    const float3 Normal = normalize(NormalSample.rgb);
    const float Roughness = RoughnessOcclusion.r;
    const float Occlusion = RoughnessOcclusion.g;
    const float3 ToView = normalize(CameraPosition.xyz - WorldPosition);

    ForwardLightSample Light;
    Light.Direct = float3(0.0, 0.0, 0.0);
    if (Direct.Color.a > 0.0)
    {
        const float3 ToLight = normalize(-Direct.Direction.xyz);
        if (dot(Normal, ToLight) > 0.0)
        {
            const float Visibility = DirectShadowVisibility(WorldPosition, Normal);
            const float3 Radiance = Direct.Color.rgb * Direct.Color.a * Visibility;
            Light.Direct = ShadeDeferredGgx(Albedo, Metallic, Roughness, Normal, ToView, ToLight, Radiance);
        }
    }

    Light.Point = float3(0.0, 0.0, 0.0);
    for (int Index = 0; Index < ForwardPointLightCapacity; ++Index)
    {
        const ForwardPointLight PointLight = Point[Index];
        if (PointLight.Color.a <= 0.0)
        {
            continue;
        }

        const float3 Offset = PointLight.Position.xyz - WorldPosition;
        const float Distance2 = dot(Offset, Offset);
        const float Radius = max(PointLight.Position.w, 0.001);
        const float Radius2 = Radius * Radius;
        if (Distance2 >= Radius2)
        {
            continue;
        }

        const float Distance = sqrt(Distance2);
        const float3 ToLight = Offset / max(Distance, 0.001);
        if (dot(Normal, ToLight) <= 0.0)
        {
            continue;
        }

        const float Falloff = Distance2 / Radius2;
        const float Window = saturate(1.0 - Falloff * Falloff);
        const float Attenuation = Window * Window / (Distance2 + 1.0);
        const float Visibility = PointShadowVisibility(Index, WorldPosition, PointLight.Position.xyz, Radius, Normal);
        const float3 Radiance = PointLight.Color.rgb * PointLight.Color.a * Attenuation * Visibility;
        Light.Point += ShadeDeferredGgx(Albedo, Metallic, Roughness, Normal, ToView, ToLight, Radiance);
    }

    Light.Area = float3(0.0, 0.0, 0.0);
    for (int AreaIndex = 0; AreaIndex < ForwardAreaLightCapacity; ++AreaIndex)
    {
        const ForwardAreaLight AreaLight = Area[AreaIndex];
        if (AreaLight.Color.a <= 0.0)
        {
            continue;
        }

        const float3 Direction = normalize(AreaLight.Direction.xyz);
        const float3 Tangent = normalize(AreaLight.Tangent.xyz);
        const float3 Bitangent = cross(Direction, Tangent);
        const float3 ToCenter = WorldPosition - AreaLight.Position.xyz;
        if (dot(ToCenter, Direction) <= 0.0)
        {
            continue;
        }

        const float HalfWidth = AreaLight.Direction.w * 0.5;
        const float HalfHeight = AreaLight.Tangent.w * 0.5;
        const float3 Local = ToCenter - Direction * dot(ToCenter, Direction);
        const float U = clamp(dot(Local, Tangent), -HalfWidth, HalfWidth);
        const float V = clamp(dot(Local, Bitangent), -HalfHeight, HalfHeight);
        const float3 Closest = AreaLight.Position.xyz + Tangent * U + Bitangent * V;
        const float3 Offset = WorldPosition - Closest;
        const float Distance2 = dot(Offset, Offset);
        const float Radius = max(AreaLight.Position.w, 0.001);
        const float Radius2 = Radius * Radius;
        if (Distance2 >= Radius2)
        {
            continue;
        }

        const float Distance = sqrt(Distance2);
        const float3 ToLight = Offset / max(Distance, 0.001);
        if (dot(Normal, ToLight) <= 0.0)
        {
            continue;
        }

        const float Falloff = Distance2 / Radius2;
        const float Window = saturate(1.0 - Falloff * Falloff);
        const float Attenuation = Window * Window / (Distance2 + 1.0);
        const float Cosine = saturate(dot(Direction, -ToLight));
        const float FarPlane = max(Radius + 0.5 * length(float2(AreaLight.Direction.w, AreaLight.Tangent.w)), 0.15);
        const float Visibility = AreaShadowVisibility(AreaIndex, WorldPosition, AreaLight.Position.xyz, Direction, Tangent, FarPlane, Normal);
        const float3 Radiance = AreaLight.Color.rgb * AreaLight.Color.a * Attenuation * Cosine * Visibility;
        Light.Area += ShadeDeferredGgx(Albedo, Metallic, Roughness, Normal, ToView, ToLight, Radiance);
    }

    Light.Sky = EvaluateIBL(
        Albedo,
        Metallic,
        Roughness,
        Occlusion,
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
    return float4(Light.Direct + Light.Point + Light.Area + Light.Sky, 1.0);
}
