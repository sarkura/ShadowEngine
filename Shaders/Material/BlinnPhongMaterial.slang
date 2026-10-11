#include "../Common/BlinnPhongConstants.slang"
#include "BlinnPhongMaterialSample.slang"

BlinnPhongMaterialSample EvaluateMaterial(VertexOutput Input)
{
    BlinnPhongMaterialSample Sample;
    Sample.Albedo = saturate(Surface.BaseColor.rgb);
    Sample.Roughness = saturate(Surface.Roughness);
    Sample.SpecularColor = Surface.SpecularColor.rgb;
    Sample.Normal = normalize(Input.WorldNormal);
    return Sample;
}
