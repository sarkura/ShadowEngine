#ifndef MATERIAL_SURFACE_DEFINED
#define MATERIAL_SURFACE_DEFINED
struct MaterialSurfaceData
{
    float3 BaseColor;
    float Roughness;
    float3 SpecularColor;
};
#endif

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
