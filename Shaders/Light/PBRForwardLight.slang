#include "../Common/ForwardConstants.slang"
#include "PBRForwardLightSample.slang"

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
    return Material.Albedo * Light * DiffuseTerm + Material.SpecularColor * Light * SpecularTerm;
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
    return Material.Albedo * Light * DiffuseTerm + Material.SpecularColor * Light * SpecularTerm;
}

PBRForwardLightSample EvaluateLight(ForwardMaterialSample Material, VertexOutput Input)
{
    const float3 Normal = normalize(Material.Normal);
    const float3 ToView = normalize(PerObject.CameraPosition.xyz - Input.WorldPosition);
    PBRForwardLightSample Sample;
    Sample.Direct = ShadeDirectional(Material, Normal, ToView);
    Sample.Point = ShadePoint(Material, Normal, Input.WorldPosition, ToView);
    Sample.Sky = Material.Albedo * Sky.Color.rgb * Sky.Color.a;
    return Sample;
}
