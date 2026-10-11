#pragma once
#pragma pack_matrix(column_major)

#include "PerObject.slang"
#include "SceneConstants.slang"
#include "MaterialSurface.slang"

#ifndef MATERIAL_DESCRIPTOR_INDICES_DEFINED
#define MATERIAL_DESCRIPTOR_INDICES_DEFINED
struct MaterialDescriptorIndices
{
    uint Padding0;
    uint Padding1;
    uint Padding2;
    uint Padding3;
    uint Padding4;
    uint Padding5;
    uint Padding6;
    uint Padding7;
};
#endif

cbuffer SceneConstants : register(b0)
{
    PerObjectData PerObject;
    MaterialSurfaceData Surface;
    MaterialDescriptorIndices Descriptors;
};
