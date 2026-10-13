#include "PostConstants.slang"

static const float ExposureMiddleGrey = 0.18;
static const float ExposureMinimum = 0.05;
static const float ExposureMaximum = 8.0;
static const float ExposureAdaptSpeed = 2.0;

[shader("fragment")]
float4 PixelMain(float4 Position : SV_Position) : SV_Target
{
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

    return float4(Exposure, 0.0, 0.0, 1.0);
}
