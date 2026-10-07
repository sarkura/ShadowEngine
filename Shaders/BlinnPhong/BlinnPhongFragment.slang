#include "../Common/BlinnPhongConstants.slang"

[shader("fragment")]
float4 PixelMain(VertexOutput Input) : SV_Target
{
    const BlinnPhongMaterialSample Material = EvaluateMaterial(Input);
    const float3 Normal = normalize(Input.WorldNormal);
    const float3 ToLight = normalize(-PerObject.LightDirection.xyz);
    const float3 ToView = normalize(PerObject.CameraPosition.xyz - Input.WorldPosition);
    const float3 HalfVector = normalize(ToLight + ToView);
    const float DiffuseTerm = saturate(dot(Normal, ToLight));
    const float Shininess = lerp(128.0, 4.0, Material.Roughness);
    const float SpecularTerm = pow(saturate(dot(Normal, HalfVector)), Shininess) * DiffuseTerm;

    const float3 Light = PerObject.LightColor.rgb * PerObject.LightColor.a;
    const float3 Ambient = Material.Albedo * 0.08;
    const float3 Diffuse = Material.Albedo * Light * DiffuseTerm;
    const float3 Specular = Material.SpecularColor * Light * SpecularTerm;
    return float4(Ambient + Diffuse + Specular, 1.0);
}
