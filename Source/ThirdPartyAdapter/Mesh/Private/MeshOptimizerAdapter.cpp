#include "ThirdPartyAdapter/Mesh/Public/MeshOptimizerAdapter.h"

#include <meshoptimizer.h>

#include <vector>

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

    bool MeshOptimizerAdapter::BuildMeshlets(
        const uint32* Indices,
        size_t IndexCount,
        const float* Positions,
        size_t VertexCount,
        size_t PositionStride,
        std::vector<uint32>& OutIndices,
        std::vector<MeshletRecord>& OutMeshlets)
    {
        OutIndices.clear();
        OutMeshlets.clear();
        constexpr size_t MaxVertices = 64;
        constexpr size_t MaxTriangles = 124;
        if (Indices == nullptr || Positions == nullptr || IndexCount < 3 || IndexCount % 3 != 0 || VertexCount == 0)
        {
            return false;
        }

        const size_t MeshletBound = meshopt_buildMeshletsBound(IndexCount, MaxVertices, MaxTriangles);
        std::vector<meshopt_Meshlet> Meshlets(MeshletBound);
        std::vector<unsigned int> MeshletVertices(IndexCount);
        std::vector<unsigned char> MeshletTriangles(IndexCount);
        const size_t MeshletCount = meshopt_buildMeshlets(
            Meshlets.data(),
            MeshletVertices.data(),
            MeshletTriangles.data(),
            Indices,
            IndexCount,
            Positions,
            VertexCount,
            PositionStride,
            MaxVertices,
            MaxTriangles,
            0.0F);
        if (MeshletCount == 0)
        {
            return false;
        }

        OutIndices.reserve(IndexCount);
        OutMeshlets.reserve(MeshletCount);
        for (size_t MeshletIndex = 0; MeshletIndex < MeshletCount; ++MeshletIndex)
        {
            meshopt_Meshlet& Source = Meshlets[MeshletIndex];
            if (Source.vertex_count == 0 || Source.triangle_count == 0 || Source.vertex_count > MaxVertices || Source.triangle_count > MaxTriangles ||
                Source.vertex_offset + Source.vertex_count > MeshletVertices.size() ||
                Source.triangle_offset + static_cast<unsigned int>(Source.triangle_count * 3) > MeshletTriangles.size())
            {
                return false;
            }

            unsigned int* Vertices = MeshletVertices.data() + Source.vertex_offset;
            unsigned char* Triangles = MeshletTriangles.data() + Source.triangle_offset;
            meshopt_optimizeMeshlet(Vertices, Triangles, Source.triangle_count, Source.vertex_count);
            const meshopt_Bounds Bounds = meshopt_computeMeshletBounds(
                Vertices,
                Triangles,
                Source.triangle_count,
                Positions,
                VertexCount,
                PositionStride);

            MeshletRecord Record;
            Record.IndexOffset = static_cast<uint32>(OutIndices.size());
            Record.IndexCount = Source.triangle_count * 3;
            Record.VertexCount = Source.vertex_count;
            Record.Center[0] = Bounds.center[0];
            Record.Center[1] = Bounds.center[1];
            Record.Center[2] = Bounds.center[2];
            Record.Radius = Bounds.radius;
            Record.ConeApex[0] = Bounds.cone_apex[0];
            Record.ConeApex[1] = Bounds.cone_apex[1];
            Record.ConeApex[2] = Bounds.cone_apex[2];
            Record.ConeAxis[0] = Bounds.cone_axis[0];
            Record.ConeAxis[1] = Bounds.cone_axis[1];
            Record.ConeAxis[2] = Bounds.cone_axis[2];
            Record.ConeCutoff = Bounds.cone_cutoff;

            for (unsigned int Triangle = 0; Triangle < Source.triangle_count; ++Triangle)
            {
                const unsigned char Local0 = Triangles[Triangle * 3];
                const unsigned char Local1 = Triangles[Triangle * 3 + 1];
                const unsigned char Local2 = Triangles[Triangle * 3 + 2];
                if (Local0 >= Source.vertex_count || Local1 >= Source.vertex_count || Local2 >= Source.vertex_count)
                {
                    return false;
                }

                const unsigned int Index0 = Vertices[Local0];
                const unsigned int Index1 = Vertices[Local1];
                const unsigned int Index2 = Vertices[Local2];
                if (Index0 >= VertexCount || Index1 >= VertexCount || Index2 >= VertexCount)
                {
                    return false;
                }

                OutIndices.push_back(Index0);
                OutIndices.push_back(Index1);
                OutIndices.push_back(Index2);
            }

            OutMeshlets.push_back(Record);
        }

        return OutIndices.size() == IndexCount;
    }
}
