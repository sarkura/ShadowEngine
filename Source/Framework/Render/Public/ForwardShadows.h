#pragma once

#include "Framework/Render/Public/ShadowTechnique.h"
#include "Framework/RHI/Public/RHIEnums.h"

#include <memory>
#include <string>
#include <vector>

namespace ShadowEngine
{
    class RHIBuffer;
    class RHIPipeline;
    class RHISampler;
    class RHITexture;

    class ForwardShadows final : public ShadowTechnique
    {
        public:
            ForwardShadows();
            ~ForwardShadows() override;

            bool Ensure(RHIDevice& Device, ShaderManager& Shaders, std::string* ErrorMessage) override;

            bool Prepare(
                const std::vector<MeshBatch>& Batches,
                const std::vector<MeshRenderProxy>& Casters,
                const AssetManager& Assets,
                const Camera& View,
                const DirectLightRenderProxy* DirectLight,
                const std::vector<PointLightRenderProxy>& PointLights,
                std::string* ErrorMessage) override;

            bool Record(
                RHICommandList& Commands,
                const std::vector<MeshBatch>& Batches,
                const std::vector<MeshRenderProxy>& Casters) override;

            [[nodiscard]] const ForwardShadowBinding& GetBinding() const override
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
