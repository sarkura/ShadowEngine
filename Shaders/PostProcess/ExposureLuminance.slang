#include "../Common/PostConstants.slang"

static const float ExposureLuminanceLimit = 8.0;

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
    const SamplerState ClampSampler = LoadSamplerDescriptor<SamplerState>(Sampler);
    const float2 Tile = 1.0 / max(DestSize, float2(1.0, 1.0));
    const float2 Origin = floor(Position) * Tile;
    float Sum = 0.0;
    [unroll]
    for (int Y = 0; Y < 4; ++Y)
    {
        [unroll]
        for (int X = 0; X < 4; ++X)
        {
            const float2 Uv = Origin + (float2(X, Y) + 0.5) * Tile * 0.25;
            const float3 SampleColor = Color.SampleLevel(ClampSampler, Uv, 0).rgb;
            const float Luminance = min(dot(SampleColor, float3(0.2126, 0.7152, 0.0722)), ExposureLuminanceLimit);
            Sum += log(max(Luminance, 0.0001));
        }
    }

    RWTexture2D<float4> OutputTexture = LoadResourceDescriptor<RWTexture2D<float4>>(Output);
    OutputTexture[DispatchThreadID.xy] = float4(Sum / 16.0, 0.0, 0.0, 1.0);
}
