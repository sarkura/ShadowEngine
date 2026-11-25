#include "../Common/PostConstants.slang"

static const int ExposureGrid = 16;

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    if (DispatchThreadID.x >= (uint)DestSize.x || DispatchThreadID.y >= (uint)DestSize.y)
    {
        return;
    }

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

    RWTexture2D<float4> OutputTexture = LoadResourceDescriptor<RWTexture2D<float4>>(Output);
    OutputTexture[DispatchThreadID.xy] = float4(Sum / float(ExposureGrid * ExposureGrid), 0.0, 0.0, 1.0);
}
