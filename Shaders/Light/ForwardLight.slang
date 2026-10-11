#include "ForwardLightSample.slang"

float3 ShadeDirectional(ForwardMaterialSample Material, float3 Normal, float3 ToView)
{
    if (Direct.Color.a <= 0.0)
    {
        return float3(0.0, 0.0, 0.0);
    }

    const float3 ToLight = normalize(-Direct.Direction.xyz);
    const float DiffuseTerm = saturate(dot(Normal, ToLight));
    const float3 HalfVector = normalize(ToLight + ToView);
    const float Shininess = lerp(128.0, 4.0, Material.Roughness);
    const float SpecularTerm = pow(saturate(dot(Normal, HalfVector)), Shininess) * DiffuseTerm;
    const float3 Light = Direct.Color.rgb * Direct.Color.a;
    const float3 SpecularColor = lerp(float3(0.04, 0.04, 0.04), Material.Albedo, Material.Metallic);
    return Material.Albedo * Light * DiffuseTerm + SpecularColor * Light * SpecularTerm;
}

float3 ShadePoint(ForwardMaterialSample Material, float3 Normal, float3 WorldPosition, float3 ToView)
{
    if (Point.Color.a <= 0.0)
    {
        return float3(0.0, 0.0, 0.0);
    }

    const float3 Offset = Point.Position.xyz - WorldPosition;
    const float Distance = length(Offset);
    const float Radius = max(Point.Position.w, 0.001);
    const float Window = saturate(1.0 - Distance / Radius);
    const float Attenuation = Window * Window;
    const float3 ToLight = Offset / max(Distance, 0.001);
    const float DiffuseTerm = saturate(dot(Normal, ToLight));
    const float3 HalfVector = normalize(ToLight + ToView);
    const float Shininess = lerp(128.0, 4.0, Material.Roughness);
    const float SpecularTerm = pow(saturate(dot(Normal, HalfVector)), Shininess) * DiffuseTerm;
    const float3 Light = Point.Color.rgb * Point.Color.a * Attenuation;
    const float3 SpecularColor = lerp(float3(0.04, 0.04, 0.04), Material.Albedo, Material.Metallic);
    return Material.Albedo * Light * DiffuseTerm + SpecularColor * Light * SpecularTerm;
}

ForwardLightSample EvaluateLight(ForwardMaterialSample Material, VertexOutput Input)
{
    const float3 Normal = normalize(Material.Normal);
    const float3 ToView = normalize(PerObject.CameraPosition.xyz - Input.WorldPosition);
    ForwardLightSample Sample;
    Sample.Direct = ShadeDirectional(Material, Normal, ToView);
    Sample.Point = ShadePoint(Material, Normal, Input.WorldPosition, ToView);
    Sample.Sky = Material.Albedo * Sky.Color.rgb * Sky.Color.a * Material.Occlusion;
    return Sample;
}
