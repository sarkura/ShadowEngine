#pragma once

#include "Framework/Render/Public/ForwardRenderPass.h"
#include "Framework/Render/Public/RenderItem.h"
#include "Framework/Render/Public/RenderQueue.h"
#include "Framework/Render/Public/Renderer.h"

namespace ShadowEngine
{
    class ForwardRenderer final : public Renderer
    {
        public:
            ForwardRenderer() = default;

            bool RenderFrame() override;

        protected:
            const char* GetName() const override;

        private:
            RenderItemFilter ItemFilter;
            RenderQueue DrawQueue;
            ForwardRenderPass Pass;
    };
}
