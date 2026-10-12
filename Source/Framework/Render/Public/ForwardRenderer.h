#pragma once

#include "Framework/Render/Public/EnvironmentCube.h"
#include "Framework/Render/Public/ForwardRenderPass.h"
#include "Framework/Render/Public/ImageBasedLighting.h"
#include "Framework/Render/Public/PostProcess.h"
#include "Framework/Render/Public/RenderItem.h"
#include "Framework/Render/Public/RenderQueue.h"
#include "Framework/Render/Public/Renderer.h"
#include "Framework/RHI/Public/RHIEnums.h"

namespace ShadowEngine
{
    class ForwardRenderer final : public Renderer
    {
        public:
            ForwardRenderer() = default;

            bool RenderFrame() override;

        protected:
            const char* GetName() const override;
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
            RenderItemFilter ItemFilter;
            RenderQueue DrawQueue;
            ForwardRenderPass Pass;
            EnvironmentCube EnvironmentCubeMap;
            PostProcess Post;
            ImageBasedLighting Lighting;
            std::unique_ptr<RHIPipeline> DepthPrepassPipeline;
            float PreviousViewProjection[16] = {};
            std::vector<float> PreviousWorlds;
            bool bHasPrevious = false;
            uint32 FrameIndex = 1;
    };
}
