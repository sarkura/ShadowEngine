#pragma once

#include "Framework/Asset/Public/AssetHandle.h"
#include "Framework/Asset/Public/Description.h"
#include "Framework/RHI/Public/RHIBuffer.h"
#include "Framework/RHI/Public/RHICommandList.h"
#include "Framework/RHI/Public/RHIPipeline.h"
#include "Framework/RHI/Public/RHIPipelineLayout.h"
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
        std::unique_ptr<RHIBuffer> MeshletVertexIds;
        std::unique_ptr<RHIBuffer> MeshletIndices;
        std::unique_ptr<RHIBuffer> Meshlets;
        std::unique_ptr<RHIPipeline> Pipeline;
        std::unique_ptr<RHIPipeline> DepthPipeline;
        std::unique_ptr<RHIPipeline> DirectionalShadowPipeline;
        std::vector<uint8> ShadowMaterial;
        EMaterialSurface Surface = EMaterialSurface::Opaque;
        bool bTwoSided = false;
        RHIPipelineLayout Layout;
        uint32 MeshletCount = 0;
    };

    inline void DrawMeshBatch(
        RHICommandList& Commands,
        const MeshBatch& Batch,
        RHIPipeline& Pipeline,
        RHIBuffer& Constants,
        uint32 ConstantOffset)
    {
        Commands.SetPipeline(Pipeline);
        Commands.BindShaderResources();
        Commands.SetConstantBuffer(Constants, ConstantOffset);
        Commands.DrawMeshlets(
            *Batch.VertexBuffer,
            *Batch.MeshletVertexIds,
            *Batch.MeshletIndices,
            *Batch.Meshlets,
            Batch.MeshletCount);
    }

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
