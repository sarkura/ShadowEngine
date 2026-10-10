#pragma once

#include "Framework/Asset/Public/AssetHandle.h"
#include "Framework/RHI/Public/RHIBuffer.h"
#include "Framework/RHI/Public/RHIPipeline.h"
#include "Framework/RHI/Public/RHIShader.h"

#include <memory>
#include <vector>

namespace ShadowEngine
{
    struct MeshBatch
    {
        MeshHandle Mesh;
        uint32 Section = 0;
        RHIShader* VertexShader = nullptr;
        RHIShader* PixelShader = nullptr;
        std::unique_ptr<RHIBuffer> VertexBuffer;
        std::unique_ptr<RHIBuffer> IndexBuffer;
        std::unique_ptr<RHIPipeline> Pipeline;
        uint32 IndexCount = 0;
    };

    inline const MeshBatch* FindMeshBatch(const std::vector<MeshBatch>& Batches, MeshHandle Mesh, uint32 Section)
    {
        for (const MeshBatch& Batch : Batches)
        {
            if (Batch.Mesh.Index == Mesh.Index && Batch.Section == Section)
            {
                return &Batch;
            }
        }

        return nullptr;
    }
}
