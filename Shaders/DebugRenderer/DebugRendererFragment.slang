#include "../Material/DebugRendererMaterialSample.slang"

[shader("fragment")]
float4 PixelMain(VertexOutput Input) : SV_Target
{
    return EvaluateMaterial(Input);
}
