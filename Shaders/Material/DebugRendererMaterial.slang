#include "DebugRendererMaterialSample.slang"

float4 EvaluateMaterial(VertexOutput Input)
{
    const float3 Normal = normalize(Input.WorldNormal);
    return float4(Normal * 0.5 + 0.5, 1.0);
}
