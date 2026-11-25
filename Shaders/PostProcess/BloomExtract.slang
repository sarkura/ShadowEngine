#include "../Common/PostConstants.slang"

float3 SampleSource(float2 Uv)
{
    const Texture2D Color = LoadResourceDescriptor<Texture2D>(Source);
    const SamplerState ClampSampler = LoadSamplerDescriptor<SamplerState>(Sampler);
    return Color.SampleLevel(ClampSampler, Uv, 0).rgb;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    if (DispatchThreadID.x >= (uint)DestSize.x || DispatchThreadID.y >= (uint)DestSize.y)
    {
        return;
    }

    const float2 Position = float2(DispatchThreadID.xy) + 0.5;
    const float2 Uv = Position / DestSize;
    const float3 Color = SampleSource(Uv);
    const float Brightness = max(Color.r, max(Color.g, Color.b));
    const float Excess = max(Brightness - Threshold, 0.0);
    const float3 Extracted = Color * Excess / max(Brightness, 0.0001);
    const float ExtractedBrightness = max(Extracted.r, max(Extracted.g, Extracted.b));
    const float LimitScale = ExtractedBrightness > BloomLimit ? BloomLimit / ExtractedBrightness : 1.0;
    RWTexture2D<float4> OutputTexture = LoadResourceDescriptor<RWTexture2D<float4>>(Output);
    OutputTexture[DispatchThreadID.xy] = float4(Extracted * LimitScale, 1.0);
}
