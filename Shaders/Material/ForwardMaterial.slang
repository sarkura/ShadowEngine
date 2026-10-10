#include "../Common/ForwardConstants.slang"
#include "ForwardMaterialSample.slang"

ForwardMaterialSample EvaluateMaterial(VertexOutput Input)
{
    ForwardMaterialSample Sample;
    Sample.Albedo = saturate(Surface.BaseColor.rgb);
    Sample.Roughness = saturate(Surface.Roughness.x);
    Sample.SpecularColor = Surface.SpecularColor.rgb;
    Sample.Normal = normalize(Input.WorldNormal);
    return Sample;
}
