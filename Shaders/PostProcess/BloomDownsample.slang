#include "../Common/PostConstants.slang"

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
    const float2 Uv = Position / DestSize;
    const float2 Texel = SourceTexel;
    const float3 A = Color.SampleLevel(ClampSampler, Uv + Texel * float2(-1.0, -1.0), 0).rgb;
    const float3 B = Color.SampleLevel(ClampSampler, Uv + Texel * float2(1.0, -1.0), 0).rgb;
    const float3 C = Color.SampleLevel(ClampSampler, Uv + Texel * float2(-1.0, 1.0), 0).rgb;
    const float3 D = Color.SampleLevel(ClampSampler, Uv + Texel * float2(1.0, 1.0), 0).rgb;
    RWTexture2D<float4> OutputTexture = LoadResourceDescriptor<RWTexture2D<float4>>(Output);
    OutputTexture[DispatchThreadID.xy] = float4((A + B + C + D) * 0.25, 1.0);
}
