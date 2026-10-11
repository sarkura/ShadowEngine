#ifndef MATERIAL_SURFACE_DEFINED
#define MATERIAL_SURFACE_DEFINED
struct MaterialSurfaceData
{
    float3 BaseColor;
    float Metallic;
    float Roughness;
};
#endif

#include "../Common/ForwardTexturedConstants.slang"
#include "ForwardMaterialSample.slang"
#include "../Common/NormalMapping.slang"
#include "../Common/DescriptorHeap.slang"

ForwardMaterialSample EvaluateMaterial(VertexOutput Input)
{
    ForwardMaterialSample Sample;
    const Texture2D BaseColorTexture = LoadResourceDescriptor<Texture2D>(Descriptors.BaseColorTexture);
    const Texture2D RoughnessTexture = LoadResourceDescriptor<Texture2D>(Descriptors.RoughnessTexture);
    const Texture2D MetallicTexture = LoadResourceDescriptor<Texture2D>(Descriptors.MetallicTexture);
    const Texture2D NormalTexture = LoadResourceDescriptor<Texture2D>(Descriptors.NormalTexture);
    const Texture2D OcclusionTexture = LoadResourceDescriptor<Texture2D>(Descriptors.OcclusionTexture);
    const SamplerState MaterialSampler = LoadSamplerDescriptor<SamplerState>(Descriptors.Sampler);
    const float3 SampledColor = BaseColorTexture.Sample(MaterialSampler, Input.TexCoord).rgb;
    const float SampledRoughness = RoughnessTexture.Sample(MaterialSampler, Input.TexCoord).r;
    const float SampledMetallic = MetallicTexture.Sample(MaterialSampler, Input.TexCoord).r;
    const float SampledOcclusion = OcclusionTexture.Sample(MaterialSampler, Input.TexCoord).r;
    const float3 TangentNormal = NormalTexture.Sample(MaterialSampler, Input.TexCoord).rgb * 2.0 - 1.0;
    Sample.Albedo = saturate(SampledColor * Surface.BaseColor.rgb);
    Sample.Metallic = saturate(SampledMetallic * Surface.Metallic);
    Sample.Roughness = saturate(SampledRoughness * Surface.Roughness);
    Sample.Occlusion = saturate(SampledOcclusion);
    Sample.Emissive = 0.0;
    Sample.Normal = ApplyNormalMap(Input.WorldNormal, Input.WorldPosition, Input.TexCoord, TangentNormal);
    return Sample;
}
