#include "ShadowConstants.slang"

[shader("vertex")]
float4 VertexMain(ShadowVertexInput Input) : SV_Position
{
    return mul(ViewProjection, mul(World, float4(Input.Position, 1.0)));
}

[shader("fragment")]
void PixelMain()
{
}
