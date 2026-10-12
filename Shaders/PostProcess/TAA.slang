#include "PostConstants.slang"

float3 SampleCurrent(float2 Pixel)
{
    const Texture2D Current = LoadResourceDescriptor<Texture2D>(Source);
    return Current.Load(int3(int2(Pixel), 0)).rgb;
}

float2 SampleMotion(float2 Pixel)
{
    const Texture2D MotionTexture = LoadResourceDescriptor<Texture2D>(Motion);
    return MotionTexture.Load(int3(int2(Pixel), 0)).xy;
}

[shader("fragment")]
float4 PixelMain(float4 Position : SV_Position) : SV_Target
{
    const float2 Pixel = Position.xy;
    const float3 Current = SampleCurrent(Pixel);
    if (CurrentWeight >= 1.0)
    {
        return float4(Current, 1.0);
    }

    float3 MinColor = Current;
    float3 MaxColor = Current;
    [unroll]
    for (int Y = -1; Y <= 1; ++Y)
    {
        [unroll]
        for (int X = -1; X <= 1; ++X)
        {
            const float3 Neighbor = SampleCurrent(Pixel + float2(X, Y));
            MinColor = min(MinColor, Neighbor);
            MaxColor = max(MaxColor, Neighbor);
        }
    }

    const float2 MotionVector = SampleMotion(Pixel);
    const float2 HistoryUv = Pixel / DestSize + MotionVector;
    const SamplerState ClampSampler = LoadSamplerDescriptor<SamplerState>(Sampler);
    const Texture2D History = LoadResourceDescriptor<Texture2D>(Secondary);
    const float3 HistoryColor = clamp(History.SampleLevel(ClampSampler, HistoryUv, 0).rgb, MinColor, MaxColor);
    return float4(lerp(HistoryColor, Current, CurrentWeight), 1.0);
}
