#include "../Material/ForwardMaterialSample.slang"
#include "../Light/ForwardLightSample.slang"

[shader("fragment")]
float4 PixelMain(VertexOutput Input, bool bFrontFace : SV_IsFrontFace) : SV_Target
{
    ForwardMaterialSample Material = EvaluateMaterial(Input);
    if (!bFrontFace)
    {
        Material.Normal = -Material.Normal;
    }

    if (Material.CutoutThreshold > 0.0)
    {
        clip(Material.Opacity - Material.CutoutThreshold);
    }

    const ForwardLightSample Light = EvaluateLight(Material, Input);
    const float Alpha = Material.CutoutThreshold > 0.0 ? 1.0 : Material.Opacity;
    return float4(Light.Direct + Light.Point + Light.Sky + Material.Emissive, Alpha);
}
