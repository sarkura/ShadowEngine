#pragma once

struct VertexInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float3 Color : COLOR0;
};

struct VertexOutput
{
    float4 Position : SV_Position;
    float3 WorldPosition : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float3 Color : COLOR0;
};
