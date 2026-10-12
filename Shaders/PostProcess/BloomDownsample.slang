#include "PostConstants.slang"

[shader("fragment")]
float4 PixelMain(float4 Position : SV_Position) : SV_Target
{
    const Texture2D Color = LoadResourceDescriptor<Texture2D>(Source);
    const SamplerState ClampSampler = LoadSamplerDescriptor<SamplerState>(Sampler);
    const float2 Uv = Position.xy / DestSize;
    const float2 Texel = SourceTexel;
    const float3 A = Color.SampleLevel(ClampSampler, Uv + Texel * float2(-1.0, -1.0), 0).rgb;
    const float3 B = Color.SampleLevel(ClampSampler, Uv + Texel * float2(1.0, -1.0), 0).rgb;
    const float3 C = Color.SampleLevel(ClampSampler, Uv + Texel * float2(-1.0, 1.0), 0).rgb;
    const float3 D = Color.SampleLevel(ClampSampler, Uv + Texel * float2(1.0, 1.0), 0).rgb;
    return float4((A + B + C + D) * 0.25, 1.0);
}
