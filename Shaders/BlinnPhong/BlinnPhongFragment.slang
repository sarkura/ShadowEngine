#include "../Common/BlinnPhongConstants.slang"
#include "../Material/BlinnPhongMaterialSample.slang"
#include "../Light/BlinnPhongLightSample.slang"

[shader("fragment")]
float4 PixelMain(VertexOutput Input) : SV_Target
{
    const BlinnPhongMaterialSample Material = EvaluateMaterial(Input);
    const BlinnPhongLightSample Light = EvaluateLight(Material, Input);
    return float4(Light.Ambient + Light.Diffuse + Light.Specular, 1.0);
}
