#include "../Common/BlinnPhongConstants.slang"
#include "BlinnPhongMaterialSample.slang"
#include "../Common/NormalMapping.slang"
#include "../Common/DescriptorHeap.slang"

BlinnPhongMaterialSample EvaluateMaterial(VertexOutput Input)
{
    BlinnPhongMaterialSample Sample;
    const Texture2D BaseColorTexture = LoadResourceDescriptor<Texture2D>(Descriptors.BaseColor);
    const Texture2D RoughnessTexture = LoadResourceDescriptor<Texture2D>(Descriptors.Roughness);
    const Texture2D NormalTexture = LoadResourceDescriptor<Texture2D>(Descriptors.Normal);
    const SamplerState MaterialSampler = LoadSamplerDescriptor<SamplerState>(Descriptors.Sampler);
    const float3 SampledColor = BaseColorTexture.Sample(MaterialSampler, Input.TexCoord).rgb;
    const float SampledRoughness = RoughnessTexture.Sample(MaterialSampler, Input.TexCoord).r;
    const float3 TangentNormal = NormalTexture.Sample(MaterialSampler, Input.TexCoord).rgb * 2.0 - 1.0;
    Sample.Albedo = saturate(SampledColor * BlinnPhongSurface.BaseColor.rgb);
    Sample.Roughness = saturate(SampledRoughness * BlinnPhongSurface.Roughness.x);
    Sample.SpecularColor = BlinnPhongSurface.SpecularColor.rgb;
    Sample.Normal = ApplyNormalMap(Input.WorldNormal, Input.WorldPosition, Input.TexCoord, TangentNormal);
    return Sample;
}
