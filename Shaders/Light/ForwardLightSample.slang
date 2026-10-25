#pragma once

#include "../Material/MaterialSample.slang"

struct ForwardLightSample
{
    float3 Direct;
    float3 Point;
    float3 Sky;
};

ForwardLightSample EvaluateLight(MaterialSample Material, VertexOutput Input);
