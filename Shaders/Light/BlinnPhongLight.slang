#include "../Common/BlinnPhongConstants.slang"
#include "BlinnPhongLightSample.slang"

BlinnPhongLightSample EvaluateLight(BlinnPhongMaterialSample Material, VertexOutput Input)
{
    const float3 Normal = normalize(Material.Normal);
    const float3 ToLight = normalize(-PerObject.LightDirection.xyz);
    const float3 ToView = normalize(PerObject.CameraPosition.xyz - Input.WorldPosition);
    const float3 HalfVector = normalize(ToLight + ToView);
    const float DiffuseTerm = saturate(dot(Normal, ToLight));
    const float Shininess = lerp(128.0, 4.0, Material.Roughness);
    const float SpecularTerm = pow(saturate(dot(Normal, HalfVector)), Shininess) * DiffuseTerm;

    const float3 Light = PerObject.LightColor.rgb * PerObject.LightColor.a;
    BlinnPhongLightSample Sample;
    Sample.Ambient = Material.Albedo * 0.08;
    Sample.Diffuse = Material.Albedo * Light * DiffuseTerm;
    Sample.Specular = Material.SpecularColor * Light * SpecularTerm;
    return Sample;
}
