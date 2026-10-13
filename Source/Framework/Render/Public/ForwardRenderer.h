#pragma once

#include "Framework/Render/Public/EnvironmentCube.h"
#include "Framework/Render/Public/ForwardRenderPass.h"
#include "Framework/Render/Public/ForwardShadows.h"
#include "Framework/Render/Public/ImageBasedLighting.h"
#include "Framework/Render/Public/PostProcess.h"
#include "Framework/Render/Public/Renderer.h"
#include "Framework/RHI/Public/RHIEnums.h"

#include <vector>

namespace ShadowEngine
{
    class ForwardRenderer final : public Renderer
    {
        public:
            ForwardRenderer();

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

            bool UsesDepthPrepass() const override
            {
                return true;
            }

        private:
            ForwardRenderPass Pass;
            EnvironmentCube EnvironmentCubeMap;
            ForwardShadows Shadows;
            PostProcess Post;
            ImageBasedLighting Lighting;
            std::unique_ptr<RHIPipeline> DepthPrepassPipeline;
            std::unique_ptr<RHIPipeline> DepthPrepassTwoSidedPipeline;
            std::vector<PointLightRenderProxy> VisiblePointLights;
            float ProjectionJitterX = 0.0F;
            float ProjectionJitterY = 0.0F;
            float PreviousViewProjection[16] = {};
            std::vector<float> PreviousWorlds;
            bool bHasPrevious = false;
            uint32 FrameIndex = 1;
    };
}
