#ifndef MATERIAL_SURFACE_DEFINED
#define MATERIAL_SURFACE_DEFINED
struct MaterialSurfaceData
{
    float3 BaseColor;
    float Roughness;
    float Alpha;
};
#endif

#include "../Common/ForwardConstants.slang"
#include "ForwardMaterialSample.slang"

ForwardMaterialSample EvaluateMaterial(VertexOutput Input)
{
    ForwardMaterialSample Sample;
    Sample.Albedo = saturate(Surface.BaseColor.rgb);
    Sample.Metallic = 0.0;
    Sample.Roughness = saturate(Surface.Roughness);
    Sample.Occlusion = 1.0;
    Sample.Emissive = 0.0;
    Sample.Normal = normalize(Input.WorldNormal);
    Sample.Opacity = saturate(Surface.Alpha);
    Sample.CutoutThreshold = 0.0;
    return Sample;
}
