#pragma once

#include "Framework/Render/Public/DebugRenderPass.h"
#include "Framework/Render/Public/Renderer.h"

namespace ShadowEngine
{
    class DebugRenderer final : public Renderer
    {
        public:
            DebugRenderer()
            {
                FramePass = &Pass;
            }

        protected:
            const char* GetName() const override;

        private:
            DebugRenderPass Pass;
    };
}
