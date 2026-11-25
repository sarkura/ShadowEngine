#include "../Common/PostConstants.slang"

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    if (DispatchThreadID.x >= (uint)DestSize.x || DispatchThreadID.y >= (uint)DestSize.y)
    {
        return;
    }

    const Texture2D AverageLuminance = LoadResourceDescriptor<Texture2D>(Source);
    const Texture2D PreviousExposure = LoadResourceDescriptor<Texture2D>(Secondary);
    const SamplerState ClampSampler = LoadSamplerDescriptor<SamplerState>(Sampler);
    const float Average = exp(AverageLuminance.SampleLevel(ClampSampler, float2(0.5, 0.5), 0).r);
    const float Target = clamp(ExposureMiddleGrey / max(Average, 0.0001), ExposureMinimum, ExposureMaximum);
    float Exposure = Target;
    if (CurrentWeight < 0.5)
    {
        const float Previous = PreviousExposure.SampleLevel(ClampSampler, float2(0.5, 0.5), 0).r;
        const float Blend = saturate(1.0 - exp(-ExposureDelta * ExposureAdaptSpeed));
        Exposure = lerp(Previous, Target, Blend);
    }

    RWTexture2D<float4> OutputTexture = LoadResourceDescriptor<RWTexture2D<float4>>(Output);
    OutputTexture[DispatchThreadID.xy] = float4(Exposure, 0.0, 0.0, 1.0);
}
