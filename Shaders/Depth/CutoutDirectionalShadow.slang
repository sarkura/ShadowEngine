#include "../Material/ForwardMaterialSample.slang"

[shader("fragment")]
void PixelMain(VertexOutput Input)
{
    const ForwardMaterialSample Material = EvaluateMaterial(Input);
    if (Material.CutoutThreshold > 0.0)
    {
        clip(Material.Opacity - Material.CutoutThreshold);
    }
}
