#include "IBLConstants.slang"
#include "../Light/ImageBasedLighting.slang"

[shader("fragment")]
float4 PixelMain(float4 Position : SV_Position) : SV_Target
{
    const float3 Normal = EquirectToDirection(Position.xy / DestSize);
    const TextureCube EnvironmentMap = LoadResourceDescriptor<TextureCube>(Source);
    const SamplerState EnvironmentSampler = LoadSamplerDescriptor<SamplerState>(Sampler);
    if (Roughness <= 0.0)
    {
        return float4(ClampEnvironmentRadiance(SampleCube(EnvironmentMap, EnvironmentSampler, Normal)), 1.0);
    }

    float3 Result = float3(0.0, 0.0, 0.0);
    float Weight = 0.0;
    const uint SampleCount = 64;
    [loop]
    for (uint Index = 0; Index < SampleCount; ++Index)
    {
        const float2 Xi = Hammersley(Index, SampleCount);
        const float3 HalfVector = ImportanceSampleGGX(Xi, Normal, Roughness);
        const float3 ToLight = normalize(2.0 * dot(Normal, HalfVector) * HalfVector - Normal);
        const float NdotL = saturate(dot(Normal, ToLight));
        if (NdotL > 0.0)
        {
            Result += ClampEnvironmentRadiance(SampleCube(EnvironmentMap, EnvironmentSampler, ToLight)) * NdotL;
            Weight += NdotL;
        }
    }

    return float4(Result / max(Weight, 0.0001), 1.0);
}
