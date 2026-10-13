#include "../Material/ForwardMaterialSample.slang"

[shader("fragment")]
float PixelMain(VertexOutput Input) : SV_Depth
{
    const ForwardMaterialSample Material = EvaluateMaterial(Input);
    if (Material.CutoutThreshold > 0.0)
    {
        clip(Material.Opacity - Material.CutoutThreshold);
    }

    const float Radius = max(PerObject.CameraPosition.w, 0.001);
    return saturate(length(Input.WorldPosition - PerObject.CameraPosition.xyz) / Radius);
}
