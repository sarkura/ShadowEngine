#pragma once

#include "Framework/Render/Public/BlinnPhongRenderPass.h"
#include "Framework/Render/Public/Renderer.h"

namespace ShadowEngine
{
    class BlinnPhongRenderer final : public Renderer
    {
        public:
            BlinnPhongRenderer()
            {
                FramePass = &Pass;
            }

        protected:
            const char* GetName() const override;

        private:
            BlinnPhongRenderPass Pass;
    };
}
