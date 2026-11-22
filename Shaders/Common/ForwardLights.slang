#pragma once

static const int ForwardPointLightCapacity = 4;
static const int ForwardAreaLightCapacity = 1;

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

struct ForwardAreaLight
{
    float4 Position;
    float4 Direction;
    float4 Tangent;
    float4 Color;
};

struct ForwardSkyLight
{
    float4 Color;
};
