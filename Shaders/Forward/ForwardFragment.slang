#include "../Material/ForwardMaterialSample.slang"
#include "../Light/ForwardLightSample.slang"

[shader("fragment")]
float4 PixelMain(VertexOutput Input) : SV_Target
{
    const ForwardMaterialSample Material = EvaluateMaterial(Input);
    const ForwardLightSample Light = EvaluateLight(Material, Input);
    return float4(Light.Direct + Light.Point + Light.Sky + Material.Emissive, 1.0);
}
