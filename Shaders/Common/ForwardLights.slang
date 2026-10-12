#pragma once

static const int ForwardPointLightCapacity = 4;

struct ForwardDirectLight
{
    float4 Direction;
    float4 Color;
};

struct ForwardPointLight
{
    float4 Position;
    float4 Color;
};

struct ForwardSkyLight
{
    float4 Color;
};
