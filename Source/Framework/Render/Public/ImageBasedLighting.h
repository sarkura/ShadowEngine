#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Common/Public/Types.h"

#include <cstdint>
#include <memory>
#include <string>

namespace ShadowEngine
{
    class RHIBuffer;
    class RHICommandList;
    class RHIDevice;
    class RHIPipeline;
    class RHISampler;
    class RHITexture;
    class ShaderManager;

    struct ImageBasedLightBinding
    {
        uint32 Irradiance = 0xffffffff;
        uint32 Prefiltered0 = 0xffffffff;
        uint32 Prefiltered1 = 0xffffffff;
        uint32 Prefiltered2 = 0xffffffff;
        uint32 Prefiltered3 = 0xffffffff;
        uint32 BrdfLut = 0xffffffff;
        uint32 EnvironmentSampler = 0xffffffff;
        uint32 BrdfSampler = 0xffffffff;
    };

    class ImageBasedLighting final : public NonCopyable
    {
        public:
            bool Ensure(
                RHIDevice& Device,
                ShaderManager& Shaders,
                RHICommandList& Commands,
                uint32 EnvironmentDescriptor,
                uint32 EnvironmentSampler,
                std::string* ErrorMessage);

            [[nodiscard]] const ImageBasedLightBinding& GetBinding() const
            {
                return Binding;
            }

        private:
            struct Target
            {
                std::unique_ptr<RHITexture> Texture;
                uint32 Descriptor = 0xffffffff;
                uint32 Width = 0;
                uint32 Height = 0;
            };

            struct IblGpuConstants
            {
                float DestSize[2];
                float Roughness;
                float Pad0;
                uint32 Source;
                uint32 Sampler;
                uint32 Pad[2];
            };

            bool CreateTarget(RHIDevice& Device, Target& Out, uint32 Width, uint32 Height, std::string* ErrorMessage);
            bool CreateFullscreenPipeline(
                RHIDevice& Device,
                ShaderManager& Shaders,
                const char* PixelPath,
                std::unique_ptr<RHIPipeline>& Out,
                std::string* ErrorMessage);
            bool Draw(
                RHICommandList& Commands,
                RHITexture& TargetTexture,
                RHIPipeline& Pipeline,
                uint32 TargetWidth,
                uint32 TargetHeight,
                float Roughness,
                uint32 Source,
                uint32 Sampler,
                std::string* ErrorMessage);

            Target Irradiance;
            Target Prefiltered[4];
            Target BrdfLut;
            std::unique_ptr<RHIPipeline> IrradiancePipeline;
            std::unique_ptr<RHIPipeline> PrefilterPipeline;
            std::unique_ptr<RHIPipeline> BrdfPipeline;
            std::unique_ptr<RHIBuffer> ConstantBuffer;
            std::unique_ptr<RHISampler> ClampSampler;
            ImageBasedLightBinding Binding;
            uint32 ConstantSlot = 0;
            bool bPipelinesReady = false;
            bool bReady = false;
    };
}
