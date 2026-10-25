#include "../Material/MaterialSample.slang"

struct GBufferTargets
{
    float4 AlbedoMetal : SV_Target0;
    float4 Normal : SV_Target1;
    float4 RoughnessOcclusion : SV_Target2;
};

[shader("fragment")]
GBufferTargets PixelMain(VertexOutput Input, bool bFrontFace : SV_IsFrontFace)
{
    MaterialSample Material = EvaluateMaterial(Input);
    if (!bFrontFace)
    {
        Material.Normal = -Material.Normal;
    }

    if (Material.CutoutThreshold > 0.0)
    {
        clip(Material.Opacity - Material.CutoutThreshold);
    }

    GBufferTargets Targets;
    Targets.AlbedoMetal = float4(Material.Albedo, Material.Metallic);
    Targets.Normal = float4(normalize(Material.Normal), 0.0);
    Targets.RoughnessOcclusion = float4(Material.Roughness, Material.Occlusion, 0.0, 0.0);
    return Targets;
}
