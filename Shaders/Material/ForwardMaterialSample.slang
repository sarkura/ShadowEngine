#pragma once

#include "../Common/SceneConstants.slang"

struct ForwardMaterialSample
{
    float3 Albedo;
    float Metallic;
    float Roughness;
    float Occlusion;
    float3 Emissive;
    float3 Normal;
    float Opacity;
    float CutoutThreshold;
};

ForwardMaterialSample EvaluateMaterial(VertexOutput Input);
