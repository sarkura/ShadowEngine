#pragma once

#include "Framework/Render/Public/DeferredRenderPass.h"
#include "Framework/Render/Public/PostProcess.h"
#include "Framework/Render/Public/Renderer.h"

namespace ShadowEngine
{
    class DeferredRenderer final : public Renderer
    {
        public:
            DeferredRenderer() = default;

            bool RenderFrame() override;

        protected:
            const char* GetName() const override;

        private:
            DeferredRenderPass Pass;
            PostProcess Post;
    };
}
