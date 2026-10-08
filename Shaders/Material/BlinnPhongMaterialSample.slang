#pragma once

#include "../Common/SceneConstants.slang"

struct BlinnPhongMaterialSample
{
    float3 Albedo;
    float Roughness;
    float3 SpecularColor;
    float3 Normal;
};

BlinnPhongMaterialSample EvaluateMaterial(VertexOutput Input);
