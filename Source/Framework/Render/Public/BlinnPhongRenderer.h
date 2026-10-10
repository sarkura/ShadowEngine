#pragma once

#include "Framework/Render/Public/BlinnPhongRenderPass.h"
#include "Framework/Render/Public/RenderItem.h"
#include "Framework/Render/Public/RenderQueue.h"
#include "Framework/Render/Public/Renderer.h"

namespace ShadowEngine
{
    class BlinnPhongRenderer final : public Renderer
    {
        public:
            BlinnPhongRenderer() = default;

            bool RenderFrame() override;

        protected:
            const char* GetName() const override;

        private:
            RenderItemFilter ItemFilter;
            RenderQueue DrawQueue;
            BlinnPhongRenderPass Pass;
    };
}
