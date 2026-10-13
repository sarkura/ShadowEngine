#include "PostConstants.slang"

static const int ExposureGrid = 16;

[shader("fragment")]
float4 PixelMain(float4 Position : SV_Position) : SV_Target
{
    const Texture2D Luminance = LoadResourceDescriptor<Texture2D>(Source);
    const SamplerState ClampSampler = LoadSamplerDescriptor<SamplerState>(Sampler);
    float Sum = 0.0;
    [loop]
    for (int Y = 0; Y < ExposureGrid; ++Y)
    {
        [loop]
        for (int X = 0; X < ExposureGrid; ++X)
        {
            const float2 Uv = (float2(X, Y) + 0.5) / float(ExposureGrid);
            Sum += Luminance.SampleLevel(ClampSampler, Uv, 0).r;
        }
    }

    return float4(Sum / float(ExposureGrid * ExposureGrid), 0.0, 0.0, 1.0);
}
