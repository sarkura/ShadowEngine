#pragma once

struct ForwardShadow
{
    float4 DirectRow0;
    float4 DirectRow1;
    float4 DirectRow2;
    float4 DirectRow3;
    float4 Settings;
    float4 Level0Row0;
    float4 Level0Row1;
    float4 Level0Row2;
    float4 Level0Row3;
    float4 Level1Row0;
    float4 Level1Row1;
    float4 Level1Row2;
    float4 Level1Row3;
    float4 Level2Row0;
    float4 Level2Row1;
    float4 Level2Row2;
    float4 Level2Row3;
    float4 Level3Row0;
    float4 Level3Row1;
    float4 Level3Row2;
    float4 Level3Row3;
    float4 VirtualInfo;
};

struct ForwardShadowIndices
{
    uint DirectMap;
    uint PointMap0;
    uint PointMap1;
    uint PointMap2;
    uint PointMap3;
    uint ComparisonSampler;
    uint PageTable;
    uint PageSampler;
};
