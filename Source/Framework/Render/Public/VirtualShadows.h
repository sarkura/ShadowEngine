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

    class VirtualShadows final : public ShadowTechnique
    {
        public:
            VirtualShadows();
            ~VirtualShadows() override;

            bool Ensure(RHIDevice& Device, ShaderManager& Shaders, std::string* ErrorMessage) override;

            bool Prepare(
                const std::vector<MeshBatch>& Batches,
                const std::vector<MeshRenderProxy>& Casters,
                const AssetManager& Assets,
                const Camera& View,
                const DirectLightRenderProxy* DirectLight,
                const std::vector<PointLightRenderProxy>& PointLights,
                const std::vector<AreaLightRenderProxy>& AreaLights,
                std::string* ErrorMessage) override;

            bool Record(
                RHICommandList& Commands,
                const std::vector<MeshBatch>& Batches,
                const std::vector<MeshRenderProxy>& Casters) override;

            [[nodiscard]] const ForwardShadowBinding& GetBinding() const override
            {
                return Binding;
            }

            struct WorldCaster
            {
                const MeshBatch* Batch = nullptr;
                float World[16] = {};
                float NormalMatrix[16] = {};
                float Min[3] = {};
                float Max[3] = {};
                bool bTwoSided = false;
            };

        private:
            struct ClipLevel
            {
                float MinX = 0.0F;
                float MinY = 0.0F;
                float Size = 0.0F;
                float Near = 0.05F;
                float Far = 1.0F;
                float Clip[16] = {};
                bool bActive = false;
            };

            struct VirtualPage
            {
                uint32 Level = 0;
                uint32 X = 0;
                uint32 Y = 0;
                uint32 Physical = 0;
                float MinX = 0.0F;
                float MinY = 0.0F;
                float MaxX = 0.0F;
                float MaxY = 0.0F;
                float DrawClip[16] = {};
            };

            struct CasterBound
            {
                const MeshBatch* Batch = nullptr;
                float World[16] = {};
                float NormalMatrix[16] = {};
                float MinX = 0.0F;
                float MinY = 0.0F;
                float MaxX = 0.0F;
                float MaxY = 0.0F;
                bool bTwoSided = false;
            };

            std::unique_ptr<RHIPipeline> CreateDepthPipeline(
                RHIDevice& Device,
                ShaderManager& Shaders,
                const char* ShaderPath,
                const char* MeshShaderPath,
                ERHICullMode Cull,
                std::string* ErrorMessage);

            std::unique_ptr<RHITexture> Atlas;
            std::unique_ptr<RHITexture> PageTable;
            std::unique_ptr<RHIPipeline> DirectionalPipeline;
            std::unique_ptr<RHIPipeline> DirectionalTwoSidedPipeline;
            std::unique_ptr<RHIBuffer> ConstantBuffer;
            std::unique_ptr<RHISampler> PageSampler;
            ForwardShadowBinding Binding;
            ClipLevel Levels[4];
            std::vector<VirtualPage> Pages;
            std::vector<CasterBound> CasterBounds;
            std::vector<WorldCaster> WorldCasters;
            std::vector<uint8> PagePixels;
            PointLightRenderProxy ActivePoints[4];
            AreaLightRenderProxy ActiveAreas[1];
            float LightView[16] = {};
            uint32 AtlasDescriptor = InvalidShaderDescriptor;
            uint32 PageDescriptor = InvalidShaderDescriptor;
            uint32 PageSamplerDescriptor = InvalidShaderDescriptor;
            bool bDirectActive = false;
            bool bReady = false;
    };
}
