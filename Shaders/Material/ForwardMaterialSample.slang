#pragma once

#include "../Common/SceneConstants.slang"

struct ForwardMaterialSample
{
    float3 Albedo;
    float Roughness;
    float3 SpecularColor;
    float3 Normal;
};

ForwardMaterialSample EvaluateMaterial(VertexOutput Input);
