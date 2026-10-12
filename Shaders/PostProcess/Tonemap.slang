#include "PostConstants.slang"
#include "../Common/ColorSpace.slang"

[shader("fragment")]
float4 PixelMain(float4 Position : SV_Position) : SV_Target
{
    const Texture2D Color = LoadResourceDescriptor<Texture2D>(Source);
    const Texture2D Bloom = LoadResourceDescriptor<Texture2D>(Secondary);
    const SamplerState ClampSampler = LoadSamplerDescriptor<SamplerState>(Sampler);
    const float2 Uv = Position.xy / DestSize;
    const float3 Linear = Color.SampleLevel(ClampSampler, Uv, 0).rgb + Bloom.SampleLevel(ClampSampler, Uv, 0).rgb * Strength;
    const float3 Exposed = Linear * CurrentWeight;
    const float3 Mapped = saturate(Exposed / (Exposed + 1.0));
    return float4(LinearToSrgb(Mapped), 1.0);
}
