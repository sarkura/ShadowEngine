#pragma once

#include "Framework/Common/Public/Types.h"

#include <cstddef>

namespace ShadowEngine
{
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
    };
}
