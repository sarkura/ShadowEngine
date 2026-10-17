#include "IBLConstants.slang"
#include "../Light/ImageBasedLighting.slang"

[shader("fragment")]
float4 PixelMain(float4 Position : SV_Position) : SV_Target
{
    const float2 Uv = saturate(Position.xy / DestSize);
    const float NdotV = max(Uv.x, 0.001);
    const float2 Brdf = IntegrateBrdf(NdotV, Uv.y);
    return float4(Brdf, 0.0, 1.0);
}
