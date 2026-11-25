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
    const Texture2D Low = LoadResourceDescriptor<Texture2D>(Source);
    const Texture2D High = LoadResourceDescriptor<Texture2D>(Secondary);
    const SamplerState ClampSampler = LoadSamplerDescriptor<SamplerState>(Sampler);
    const float2 Uv = Position / DestSize;
    const float3 Color = Low.SampleLevel(ClampSampler, Uv, 0).rgb + High.SampleLevel(ClampSampler, Uv, 0).rgb;
    RWTexture2D<float4> OutputTexture = LoadResourceDescriptor<RWTexture2D<float4>>(Output);
    OutputTexture[DispatchThreadID.xy] = float4(Color, 1.0);
}
