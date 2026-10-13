#pragma once

struct ForwardShadow
{
    float4 DirectRow0;
    float4 DirectRow1;
    float4 DirectRow2;
    float4 DirectRow3;
    float4 Settings;
};

struct ForwardShadowIndices
{
    uint DirectMap;
    uint PointMap0;
    uint PointMap1;
    uint PointMap2;
    uint PointMap3;
    uint ComparisonSampler;
    uint Padding0;
    uint Padding1;
};
