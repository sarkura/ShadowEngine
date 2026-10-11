#ifndef MATERIAL_SURFACE_DEFINED
#define MATERIAL_SURFACE_DEFINED
struct MaterialSurfaceData
{
    float3 BaseColor;
    float Metallic;
    float Roughness;
    float3 SpecularColor;
};
#endif

#include "../Common/BlinnPhongTexturedConstants.slang"
#include "BlinnPhongMaterialSample.slang"
#include "../Common/NormalMapping.slang"
#include "../Common/DescriptorHeap.slang"

BlinnPhongMaterialSample EvaluateMaterial(VertexOutput Input)
{
    BlinnPhongMaterialSample Sample;
    const Texture2D BaseColorTexture = LoadResourceDescriptor<Texture2D>(Descriptors.BaseColorTexture);
    const Texture2D RoughnessTexture = LoadResourceDescriptor<Texture2D>(Descriptors.RoughnessTexture);
    const Texture2D NormalTexture = LoadResourceDescriptor<Texture2D>(Descriptors.NormalTexture);
    const SamplerState MaterialSampler = LoadSamplerDescriptor<SamplerState>(Descriptors.Sampler);
    const float3 SampledColor = BaseColorTexture.Sample(MaterialSampler, Input.TexCoord).rgb;
    const float SampledRoughness = RoughnessTexture.Sample(MaterialSampler, Input.TexCoord).r;
    const float3 TangentNormal = NormalTexture.Sample(MaterialSampler, Input.TexCoord).rgb * 2.0 - 1.0;
    Sample.Albedo = saturate(SampledColor * Surface.BaseColor.rgb);
    Sample.Roughness = saturate(SampledRoughness * Surface.Roughness);
    Sample.SpecularColor = Surface.SpecularColor.rgb;
    Sample.Normal = ApplyNormalMap(Input.WorldNormal, Input.WorldPosition, Input.TexCoord, TangentNormal);
    return Sample;
}
