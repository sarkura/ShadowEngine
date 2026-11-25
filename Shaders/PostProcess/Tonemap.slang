#include "../Common/PostConstants.slang"
#include "../Common/ColorSpace.slang"

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    if (DispatchThreadID.x >= (uint)DestSize.x || DispatchThreadID.y >= (uint)DestSize.y)
    {
        return;
    }

    const float2 Position = float2(DispatchThreadID.xy) + 0.5;
    const Texture2D Color = LoadResourceDescriptor<Texture2D>(Source);
    const Texture2D Bloom = LoadResourceDescriptor<Texture2D>(Secondary);
    const Texture2D ExposureMapTexture = LoadResourceDescriptor<Texture2D>(ExposureMap);
    const SamplerState ClampSampler = LoadSamplerDescriptor<SamplerState>(Sampler);
    const float2 Uv = Position / DestSize;
    float Exposure = 1.0;
    if ((PostFlags & PostFlagAutoExposure) != 0)
    {
        Exposure = ExposureMapTexture.SampleLevel(ClampSampler, float2(0.5, 0.5), 0).r;
    }
    float3 BloomColor = float3(0.0, 0.0, 0.0);
    if ((PostFlags & PostFlagBloom) != 0)
    {
        BloomColor = Bloom.SampleLevel(ClampSampler, Uv, 0).rgb;
    }
    const float3 Linear = Color.SampleLevel(ClampSampler, Uv, 0).rgb + BloomColor * Strength;
    const float3 Exposed = Linear * Exposure;
    const float3 Mapped = saturate(Exposed / (Exposed + 1.0));
    float3 Encoded = LinearToSrgb(Mapped);
    if ((PostFlags & PostFlagDither) != 0)
    {
        const float Noise = frac(52.9829189 * frac(dot(Position, float2(0.06711056, 0.00583715))));
        Encoded = saturate(Encoded + (Noise - 0.5) / 255.0);
    }
    RWTexture2D<float4> OutputTexture = LoadResourceDescriptor<RWTexture2D<float4>>(Output);
    OutputTexture[DispatchThreadID.xy] = float4(Encoded, 1.0);
}
