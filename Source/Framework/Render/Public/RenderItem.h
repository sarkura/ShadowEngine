#pragma once

#include "Framework/Asset/Public/AssetHandle.h"
#include "Framework/Material/Public/MaterialHandle.h"
#include "Framework/Render/Public/MeshBatch.h"
#include "Framework/Render/Public/RenderProxy.h"

#include <string_view>
#include <vector>

namespace ShadowEngine
{
    class AssetManager;
    class RenderQueue;

    struct RenderItem
    {
        MeshHandle Mesh;
        MaterialInstanceHandle Material;
        uint32 Section = 0;
        float World[16] = {};
        float NormalMatrix[16] = {};
    };

    class RenderItemFilter
    {
        public:
            virtual ~RenderItemFilter() = default;

            [[nodiscard]] virtual bool Accept(
                const MeshRenderProxy& Proxy,
                uint32 SectionIndex,
                MaterialInstanceHandle Material) const;
    };

    bool FillRenderQueue(
        const RenderProxy& Proxy,
        const RenderItemFilter& Filter,
        AssetManager& Assets,
        std::string_view RendererName,
        const std::vector<MeshBatch>& Batches,
        RenderQueue& Queue);
}
