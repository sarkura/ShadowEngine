#include "../BlinnPhong/BlinnPhongVertex.slang"

struct BlinnPhongMaterialSample
{
    float3 Albedo;
    float Roughness;
    float3 SpecularColor;
};

BlinnPhongMaterialSample EvaluateMaterial(VertexOutput Input)
{
    BlinnPhongMaterialSample Sample;
    Sample.Albedo = saturate(BlinnPhongSurface.BaseColor.rgb);
    Sample.Roughness = saturate(BlinnPhongSurface.Roughness.x);
    Sample.SpecularColor = BlinnPhongSurface.SpecularColor.rgb;
    return Sample;
}

#include "../BlinnPhong/BlinnPhongFragment.slang"
