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
    const float2 Ndc = float2(Position.x / DestSize.x * 2.0 - 1.0, 1.0 - Position.y / DestSize.y * 2.0);
    const Texture2D<float> DepthTexture = LoadResourceDescriptor<Texture2D<float>>(Depth);
    const float SceneDepth = DepthTexture.Load(int3(int2(Position.xy), 0));
    const float4 World = mul(InverseViewProjection, float4(Ndc, SceneDepth, 1.0));
    const float4 PreviousClip = mul(PreviousViewProjection, float4(World.xyz / World.w, 1.0));
    const float2 PreviousUv = PreviousClip.xy / PreviousClip.w * float2(0.5, -0.5) + 0.5;
    const float2 CurrentUv = Position / DestSize;
    RWTexture2D<float4> OutputTexture = LoadResourceDescriptor<RWTexture2D<float4>>(Output);
    OutputTexture[DispatchThreadID.xy] = float4(PreviousUv - CurrentUv, 0.0, 0.0);
}
