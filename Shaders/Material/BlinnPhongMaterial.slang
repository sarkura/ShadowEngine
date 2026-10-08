#include "../Common/BlinnPhongConstants.slang"
#include "BlinnPhongMaterialSample.slang"

BlinnPhongMaterialSample EvaluateMaterial(VertexOutput Input)
{
    BlinnPhongMaterialSample Sample;
    Sample.Albedo = saturate(BlinnPhongSurface.BaseColor.rgb);
    Sample.Roughness = saturate(BlinnPhongSurface.Roughness.x);
    Sample.SpecularColor = BlinnPhongSurface.SpecularColor.rgb;
    Sample.Normal = normalize(Input.WorldNormal);
    return Sample;
}
