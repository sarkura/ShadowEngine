#include "PostConstants.slang"

[shader("fragment")]
float4 PixelMain(float4 Position : SV_Position) : SV_Target
{
    const Texture2D Low = LoadResourceDescriptor<Texture2D>(Source);
    const Texture2D High = LoadResourceDescriptor<Texture2D>(Secondary);
    const SamplerState ClampSampler = LoadSamplerDescriptor<SamplerState>(Sampler);
    const float2 Uv = Position.xy / DestSize;
    const float3 Color = Low.SampleLevel(ClampSampler, Uv, 0).rgb + High.SampleLevel(ClampSampler, Uv, 0).rgb;
    return float4(Color, 1.0);
}
