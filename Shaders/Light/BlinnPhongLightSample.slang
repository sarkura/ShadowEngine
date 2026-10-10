#pragma once

#include "../Material/BlinnPhongMaterialSample.slang"

struct BlinnPhongLightSample
{
    float3 Ambient;
    float3 Diffuse;
    float3 Specular;
};

BlinnPhongLightSample EvaluateLight(BlinnPhongMaterialSample Material, VertexOutput Input);
