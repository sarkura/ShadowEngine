#include "PostConstants.slang"

static const float ExposureLuminanceLimit = 8.0;

[shader("fragment")]
float4 PixelMain(float4 Position : SV_Position) : SV_Target
{
    const Texture2D Color = LoadResourceDescriptor<Texture2D>(Source);
    const SamplerState ClampSampler = LoadSamplerDescriptor<SamplerState>(Sampler);
    const float2 Tile = 1.0 / max(DestSize, float2(1.0, 1.0));
    const float2 Origin = floor(Position.xy) * Tile;
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

    return float4(Sum / 16.0, 0.0, 0.0, 1.0);
}
