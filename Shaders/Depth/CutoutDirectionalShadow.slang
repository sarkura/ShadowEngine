#include "../Material/MaterialSample.slang"

[shader("fragment")]
void PixelMain(VertexOutput Input)
{
    const MaterialSample Material = EvaluateMaterial(Input);
    if (Material.CutoutThreshold > 0.0)
    {
        clip(Material.Opacity - Material.CutoutThreshold);
    }
}
