#pragma once

#include "../Common/SceneConstants.slang"

struct MaterialSample
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

MaterialSample EvaluateMaterial(VertexOutput Input);
