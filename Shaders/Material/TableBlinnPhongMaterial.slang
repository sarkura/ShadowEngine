#include "../BlinnPhong/BlinnPhongVertex.slang"

Texture2D BaseColorTexture : register(t0);
Texture2D RoughnessTexture : register(t1);
SamplerState MaterialSampler : register(s0);

struct BlinnPhongMaterialSample
{
    float3 Albedo;
    float Roughness;
    float3 SpecularColor;
};

BlinnPhongMaterialSample EvaluateMaterial(VertexOutput Input)
{
    BlinnPhongMaterialSample Sample;
    const float3 SampledColor = BaseColorTexture.Sample(MaterialSampler, Input.TexCoord).rgb;
    const float SampledRoughness = RoughnessTexture.Sample(MaterialSampler, Input.TexCoord).r;
    Sample.Albedo = saturate(SampledColor * BlinnPhongSurface.BaseColor.rgb);
    Sample.Roughness = saturate(SampledRoughness * BlinnPhongSurface.Roughness.x);
    Sample.SpecularColor = BlinnPhongSurface.SpecularColor.rgb;
    return Sample;
}

#include "../BlinnPhong/BlinnPhongFragment.slang"
