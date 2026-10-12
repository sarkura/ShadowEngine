#pragma once

#ifndef IMAGE_BASED_LIGHT_INDICES_DEFINED
#define IMAGE_BASED_LIGHT_INDICES_DEFINED
struct ImageBasedLightIndices
{
    uint Irradiance;
    uint Prefiltered0;
    uint Prefiltered1;
    uint Prefiltered2;
    uint Prefiltered3;
    uint BrdfLut;
    uint EnvironmentSampler;
    uint BrdfSampler;
};
#endif
