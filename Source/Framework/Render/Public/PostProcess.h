#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/RHI/Public/RHIEnums.h"

#include <memory>
#include <string>
#include <vector>

namespace ShadowEngine
{
    class MeshBatch;
    class RenderItem;
    class RHIBuffer;
    class RHICommandList;
    class RHIDevice;
    class RHIPipeline;
    class RHISampler;
    class RHITexture;
    class ShaderManager;

    class PostProcess final : public NonCopyable
    {
        public:
            bool Ensure(
                RHIDevice& Device,
                ShaderManager& Shaders,
                RHITexture& Depth,
                uint32 Width,
                uint32 Height,
                ERHIFormat OutputFormat,
                std::string* ErrorMessage);

            [[nodiscard]] RHITexture* GetSceneColor() const;

            bool Draw(
                RHICommandList& Commands,
                RHITexture& BackBuffer,
                RHITexture& Depth,
                RHIBuffer& ObjectConstants,
                const std::vector<RenderItem>& Items,
                const std::vector<MeshBatch>& Batches,
                const float InverseViewProjection[16],
                const float PreviousViewProjection[16],
                const float CameraPosition[3],
                uint32 Width,
                uint32 Height,
                uint32 ConstantAlignment);

        private:
            struct Target
            {
                std::unique_ptr<RHITexture> Texture;
                uint32 Descriptor = 0xffffffff;
                uint32 Width = 0;
                uint32 Height = 0;
            };

            bool CreateTarget(RHIDevice& Device, Target& Out, uint32 Width, uint32 Height, std::string* ErrorMessage);
            bool CreateFullscreenPipeline(
                RHIDevice& Device,
                ShaderManager& Shaders,
                const char* PixelPath,
                ERHIFormat Format,
                std::unique_ptr<RHIPipeline>& Out,
                std::string* ErrorMessage);

            struct PostGpuConstants
            {
                float InverseViewProjection[16];
                float PreviousViewProjection[16];
                float CameraPosition[4];
                float DestSize[2];
                float SourceTexel[2];
                float Threshold;
                float Strength;
                float CurrentWeight;
                float Pad0;
                uint32 Source;
                uint32 Secondary;
                uint32 Motion;
                uint32 Depth;
                uint32 Sampler;
                uint32 Pad1;
                uint32 Pad2;
                uint32 Pad3;
            };

            bool DrawFullscreen(
                RHICommandList& Commands,
                RHITexture& TargetTexture,
                RHIPipeline& Pipeline,
                uint32 TargetWidth,
                uint32 TargetHeight,
                const PostGpuConstants& Constants);

            Target SceneColor;
            Target Motion;
            Target History[2];
            Target BloomDown[4];
            Target BloomUp[3];
            std::unique_ptr<RHIPipeline> CameraVelocityPipeline;
            std::unique_ptr<RHIPipeline> MeshVelocityPipeline;
            std::unique_ptr<RHIPipeline> TAAPipeline;
            std::unique_ptr<RHIPipeline> BloomExtractPipeline;
            std::unique_ptr<RHIPipeline> BloomDownsamplePipeline;
            std::unique_ptr<RHIPipeline> BloomUpsamplePipeline;
            std::unique_ptr<RHIPipeline> TonemapPipeline;
            std::unique_ptr<RHIBuffer> ConstantBuffer;
            std::unique_ptr<RHISampler> ClampSampler;
            RHITexture* DepthTexture = nullptr;
            uint32 DepthDescriptor = 0xffffffff;
            uint32 Width = 0;
            uint32 Height = 0;
            uint32 HistoryIndex = 0;
            uint32 ConstantSlot = 0;
            bool bHistoryValid = false;
            bool bPipelinesReady = false;
    };
}
