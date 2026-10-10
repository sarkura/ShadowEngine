#include "../Common/ForwardConstants.slang"
#include "../Material/ForwardMaterialSample.slang"
#include "../Light/PBRForwardLightSample.slang"

[shader("fragment")]
float4 PixelMain(VertexOutput Input) : SV_Target
{
    const ForwardMaterialSample Material = EvaluateMaterial(Input);
    const PBRForwardLightSample Light = EvaluateLight(Material, Input);
    return float4(Light.Direct + Light.Point + Light.Sky, 1.0);
}
