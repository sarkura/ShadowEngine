#include "IblConstants.slang"
#include "../Light/ImageBasedLighting.slang"

[shader("fragment")]
float4 PixelMain(float4 Position : SV_Position) : SV_Target
{
    const float2 Uv = Position.xy / DestSize;
    const float3 Normal = EquirectToDirection(Uv);
    const TextureCube EnvironmentMap = LoadResourceDescriptor<TextureCube>(Source);
    const SamplerState EnvironmentSampler = LoadSamplerDescriptor<SamplerState>(Sampler);
    const float3 Up = abs(Normal.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    const float3 Tangent = normalize(cross(Up, Normal));
    const float3 Bitangent = cross(Normal, Tangent);
    float3 Result = float3(0.0, 0.0, 0.0);
    const uint SampleCount = 64;
    [loop]
    for (uint Index = 0; Index < SampleCount; ++Index)
    {
        const float2 Xi = Hammersley(Index, SampleCount);
        const float Phi = 6.283185307 * Xi.x;
        const float CosTheta = sqrt(1.0 - Xi.y);
        const float SinTheta = sqrt(max(1.0 - CosTheta * CosTheta, 0.0));
        const float3 Direction = normalize(
            Tangent * (SinTheta * cos(Phi)) + Bitangent * (SinTheta * sin(Phi)) + Normal * CosTheta);
        Result += ClampEnvironmentRadiance(SampleCube(EnvironmentMap, EnvironmentSampler, Direction));
    }

    return float4(Result / float(SampleCount), 1.0);
}
