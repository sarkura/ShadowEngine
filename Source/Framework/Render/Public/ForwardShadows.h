#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Common/Public/Types.h"
#include "Framework/Render/Public/MeshBatch.h"
#include "Framework/Render/Public/RenderProxy.h"
#include "Framework/RHI/Public/RHIEnums.h"
#include "Framework/RHI/Public/RHIPipelineLayout.h"

#include <memory>
#include <string>
#include <vector>

namespace ShadowEngine
{
    class AssetManager;
    class RHIBuffer;
    class RHICommandList;
    class RHIDevice;
    class RHIPipeline;
    class RHISampler;
    class RHITexture;
    class ShaderManager;

    struct ForwardShadowBinding
    {
        float DirectRow[4][4] = {};
        float Settings[4] = {};
        uint32 DirectMap = InvalidShaderDescriptor;
        uint32 PointMap[4] = {
            InvalidShaderDescriptor,
            InvalidShaderDescriptor,
            InvalidShaderDescriptor,
            InvalidShaderDescriptor};
        uint32 ComparisonSampler = InvalidShaderDescriptor;
    };

    class ForwardShadows final : public NonCopyable
    {
        public:
            bool Ensure(RHIDevice& Device, ShaderManager& Shaders, std::string* ErrorMessage);

            bool Prepare(
                const std::vector<MeshBatch>& Batches,
                const std::vector<MeshRenderProxy>& Casters,
                const AssetManager& Assets,
                const DirectLightRenderProxy* DirectLight,
                const std::vector<PointLightRenderProxy>& PointLights,
                std::string* ErrorMessage);

            bool Record(
                RHICommandList& Commands,
                const std::vector<MeshBatch>& Batches,
                const std::vector<MeshRenderProxy>& Casters);

            [[nodiscard]] const ForwardShadowBinding& GetBinding() const
            {
                return Binding;
            }

        private:
            std::unique_ptr<RHIPipeline> CreateDepthPipeline(
                RHIDevice& Device,
                ShaderManager& Shaders,
                const char* ShaderPath,
                int32 DepthBias,
                float SlopeScaledDepthBias,
                ERHICullMode Cull,
                std::string* ErrorMessage);

            std::unique_ptr<RHITexture> DirectionalMap;
            std::unique_ptr<RHITexture> PointMaps[4];
            std::unique_ptr<RHIPipeline> DirectionalPipeline;
            std::unique_ptr<RHIPipeline> DirectionalTwoSidedPipeline;
            std::unique_ptr<RHIPipeline> PointPipeline;
            std::unique_ptr<RHIPipeline> PointTwoSidedPipeline;
            std::unique_ptr<RHIBuffer> ConstantBuffer;
            std::unique_ptr<RHISampler> ComparisonSampler;
            ForwardShadowBinding Binding;
            PointLightRenderProxy ActivePoints[4];
            uint32 DirectDescriptor = InvalidShaderDescriptor;
            uint32 PointDescriptors[4] = {
                InvalidShaderDescriptor,
                InvalidShaderDescriptor,
                InvalidShaderDescriptor,
                InvalidShaderDescriptor};
            uint32 ComparisonDescriptor = InvalidShaderDescriptor;
            float DirectClip[16] = {};
            bool bDirectActive = false;
            bool bPointActive[4] = {};
            bool bReady = false;
    };
}
