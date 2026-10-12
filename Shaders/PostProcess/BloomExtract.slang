#include "PostConstants.slang"

float3 SampleSource(float2 Uv)
{
    const Texture2D Color = LoadResourceDescriptor<Texture2D>(Source);
    const SamplerState ClampSampler = LoadSamplerDescriptor<SamplerState>(Sampler);
    return Color.SampleLevel(ClampSampler, Uv, 0).rgb;
}

[shader("fragment")]
float4 PixelMain(float4 Position : SV_Position) : SV_Target
{
    const float2 Uv = Position.xy / DestSize;
    const float3 Color = SampleSource(Uv);
    const float Brightness = max(Color.r, max(Color.g, Color.b));
    const float Excess = max(Brightness - Threshold, 0.0);
    const float3 Extracted = Color * Excess / max(Brightness, 0.0001);
    const float ExtractedBrightness = max(Extracted.r, max(Extracted.g, Extracted.b));
    const float BloomLimit = 4.0;
    const float LimitScale = ExtractedBrightness > BloomLimit ? BloomLimit / ExtractedBrightness : 1.0;
    return float4(Extracted * LimitScale, 1.0);
}
