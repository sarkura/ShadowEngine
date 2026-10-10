#pragma once

#include "Framework/Render/Public/DebugRenderPass.h"
#include "Framework/Render/Public/RenderItem.h"
#include "Framework/Render/Public/RenderQueue.h"
#include "Framework/Render/Public/Renderer.h"

namespace ShadowEngine
{
    class DebugRenderer final : public Renderer
    {
        public:
            DebugRenderer() = default;

            bool RenderFrame() override;

        protected:
            const char* GetName() const override;

        private:
            RenderItemFilter ItemFilter;
            RenderQueue DrawQueue;
            DebugRenderPass Pass;
    };
}
