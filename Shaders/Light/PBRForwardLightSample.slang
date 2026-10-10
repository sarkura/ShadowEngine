#pragma once

#include "../Material/ForwardMaterialSample.slang"

struct PBRForwardLightSample
{
    float3 Direct;
    float3 Point;
    float3 Sky;
};

PBRForwardLightSample EvaluateLight(ForwardMaterialSample Material, VertexOutput Input);
