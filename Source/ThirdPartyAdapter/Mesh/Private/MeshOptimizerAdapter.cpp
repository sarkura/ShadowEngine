#include "ThirdPartyAdapter/Mesh/Public/MeshOptimizerAdapter.h"

#include <meshoptimizer.h>

namespace ShadowEngine
{
    void MeshOptimizerAdapter::OptimizeVertexCache(uint32* Destination, const uint32* Indices, size_t IndexCount, size_t VertexCount)
    {
        meshopt_optimizeVertexCache(Destination, Indices, IndexCount, VertexCount);
    }

    void MeshOptimizerAdapter::OptimizeOverdraw(
        uint32* Destination,
        const uint32* Indices,
        size_t IndexCount,
        const float* Positions,
        size_t VertexCount,
        size_t PositionStride)
    {
        meshopt_optimizeOverdraw(Destination, Indices, IndexCount, Positions, VertexCount, PositionStride, 1.05F);
    }

    size_t MeshOptimizerAdapter::OptimizeVertexFetch(
        void* Destination,
        uint32* Indices,
        size_t IndexCount,
        const void* Vertices,
        size_t VertexCount,
        size_t VertexSize)
    {
        return meshopt_optimizeVertexFetch(Destination, Indices, IndexCount, Vertices, VertexCount, VertexSize);
    }
}
