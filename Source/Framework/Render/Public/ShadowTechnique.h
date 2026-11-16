#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Common/Public/Types.h"
#include "Framework/Render/Public/Camera.h"
#include "Framework/Render/Public/MeshBatch.h"
#include "Framework/Render/Public/RenderProxy.h"
#include "Framework/RHI/Public/RHIPipelineLayout.h"

#include <memory>
#include <string>
#include <vector>

namespace ShadowEngine
{
    class AssetManager;
    class RHICommandList;
    class RHIDevice;
    class ShaderManager;

    struct ForwardShadowBinding
    {
        float DirectRow[4][4] = {};
        float LevelRow[4][4][4] = {};
        float Settings[4] = {};
        float VirtualInfo[4] = {};
        uint32 DirectMap = InvalidShaderDescriptor;
        uint32 PointMap[4] = {
            InvalidShaderDescriptor,
            InvalidShaderDescriptor,
            InvalidShaderDescriptor,
            InvalidShaderDescriptor};
        uint32 ComparisonSampler = InvalidShaderDescriptor;
        uint32 PageTable = InvalidShaderDescriptor;
        uint32 PageSampler = InvalidShaderDescriptor;
    };

    class ShadowTechnique : public NonCopyable
    {
        public:
            virtual ~ShadowTechnique() = default;

            virtual bool Ensure(RHIDevice& Device, ShaderManager& Shaders, std::string* ErrorMessage) = 0;

            virtual bool Prepare(
                const std::vector<MeshBatch>& Batches,
                const std::vector<MeshRenderProxy>& Casters,
                const AssetManager& Assets,
                const Camera& View,
                const DirectLightRenderProxy* DirectLight,
                const std::vector<PointLightRenderProxy>& PointLights,
                std::string* ErrorMessage) = 0;

            virtual bool Record(
                RHICommandList& Commands,
                const std::vector<MeshBatch>& Batches,
                const std::vector<MeshRenderProxy>& Casters) = 0;

            [[nodiscard]] virtual const ForwardShadowBinding& GetBinding() const = 0;
    };

    [[nodiscard]] std::unique_ptr<ShadowTechnique> CreateShadowTechnique();
}
