#pragma once

#include "../Material/ForwardMaterialSample.slang"

struct ForwardLightSample
{
    float3 Direct;
    float3 Point;
    float3 Sky;
};

ForwardLightSample EvaluateLight(ForwardMaterialSample Material, VertexOutput Input);
