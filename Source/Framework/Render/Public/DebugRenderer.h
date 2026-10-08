#pragma once

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
    };
}
