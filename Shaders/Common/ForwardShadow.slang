#pragma once

struct ForwardShadow
{
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
    uint PageTable;
    uint PageSampler;
};
