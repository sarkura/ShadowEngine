#pragma once

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
    };
}
