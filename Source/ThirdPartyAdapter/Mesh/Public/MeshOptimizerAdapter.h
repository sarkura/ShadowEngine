#pragma once

#include "Framework/Common/Public/Types.h"

#include <cstddef>
#include <vector>

namespace ShadowEngine
{
    struct MeshletRecord
    {
        uint32 IndexOffset = 0;
        uint32 IndexCount = 0;
        uint32 VertexCount = 0;
        float Center[3] = {};
        float Radius = 0.0F;
        float ConeApex[3] = {};
        float ConeAxis[3] = {};
        float ConeCutoff = 1.0F;
    };

    class MeshOptimizerAdapter final
    {
        public:
            static void OptimizeVertexCache(uint32* Destination, const uint32* Indices, size_t IndexCount, size_t VertexCount);
            static void OptimizeOverdraw(
                uint32* Destination,
                const uint32* Indices,
                size_t IndexCount,
                const float* Positions,
                size_t VertexCount,
                size_t PositionStride);
            static size_t OptimizeVertexFetch(
                void* Destination,
                uint32* Indices,
                size_t IndexCount,
                const void* Vertices,
                size_t VertexCount,
                size_t VertexSize);
            static bool BuildMeshlets(
                const uint32* Indices,
                size_t IndexCount,
                const float* Positions,
                size_t VertexCount,
                size_t PositionStride,
                std::vector<uint32>& OutIndices,
                std::vector<MeshletRecord>& OutMeshlets);
    };
}
