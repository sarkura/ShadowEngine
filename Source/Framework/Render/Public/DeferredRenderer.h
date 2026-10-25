#pragma once

#include "Framework/Render/Public/DeferredRenderPass.h"
#include "Framework/Render/Public/EnvironmentCube.h"
#include "Framework/Render/Public/ForwardShadows.h"
#include "Framework/Render/Public/ImageBasedLighting.h"
#include "Framework/Render/Public/PostProcess.h"
#include "Framework/Render/Public/Renderer.h"
#include "Framework/RHI/Public/RHIEnums.h"

#include <vector>

namespace ShadowEngine
{
    class DeferredRenderer final : public Renderer
    {
        public:
            DeferredRenderer();

        protected:
            const char* GetName() const override;
            bool PrepareFrame() override;
            void PopulateContext(RenderPassContext& Context) override;
            void StoreFrameHistory() override;
            bool DrawsSkyBackground() const override
            {
                return true;
            }

            ERHIFormat GetSceneColorFormat() const override
            {
                return ERHIFormat::R16G16B16A16_Float;
            }

        private:
            bool EnsureTargets(std::string* ErrorMessage);

            DeferredRenderPass Pass;
            EnvironmentCube EnvironmentCubeMap;
            ForwardShadows Shadows;
            PostProcess Post;
            ImageBasedLighting Lighting;
            std::unique_ptr<RHITexture> GBufferA;
            std::unique_ptr<RHITexture> GBufferB;
            std::unique_ptr<RHITexture> GBufferC;
            std::unique_ptr<RHIPipeline> LightPipeline;
            std::unique_ptr<RHIBuffer> LightConstants;
            std::unique_ptr<RHISampler> GBufferSampler;
            RHITexture* DepthResource = nullptr;
            uint32 GBufferADescriptor = 0xffffffff;
            uint32 GBufferBDescriptor = 0xffffffff;
            uint32 GBufferCDescriptor = 0xffffffff;
            uint32 DepthDescriptor = 0xffffffff;
            std::vector<PointLightRenderProxy> VisiblePointLights;
            float ProjectionJitterX = 0.0F;
            float ProjectionJitterY = 0.0F;
            float PreviousViewProjection[16] = {};
            std::vector<float> PreviousWorlds;
            bool bHasPrevious = false;
            uint32 FrameIndex = 1;
            uint32 TargetWidth = 0;
            uint32 TargetHeight = 0;
    };
}
