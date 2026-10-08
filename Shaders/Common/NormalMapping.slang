#pragma once

float3 ApplyNormalMap(float3 WorldNormal, float3 WorldPosition, float2 TexCoord, float3 TangentNormal)
{
    const float3 ShadingNormal = normalize(WorldNormal);
    const float3 PositionX = ddx(WorldPosition);
    const float3 PositionY = ddy(WorldPosition);
    const float2 CoordX = ddx(TexCoord);
    const float2 CoordY = ddy(TexCoord);
    const float3 PositionYPerp = cross(PositionY, ShadingNormal);
    const float3 PositionXPerp = cross(ShadingNormal, PositionX);
    float3 Tangent = PositionYPerp * CoordX.x + PositionXPerp * CoordY.x;
    float3 Bitangent = PositionYPerp * CoordX.y + PositionXPerp * CoordY.y;
    const float LengthSquared = max(dot(Tangent, Tangent), dot(Bitangent, Bitangent));
    if (LengthSquared < 1.0e-8)
    {
        return ShadingNormal;
    }

    const float Scale = rsqrt(LengthSquared);
    Tangent *= Scale;
    Bitangent *= Scale;
    return normalize(Tangent * TangentNormal.x + Bitangent * TangentNormal.y + ShadingNormal * TangentNormal.z);
}
