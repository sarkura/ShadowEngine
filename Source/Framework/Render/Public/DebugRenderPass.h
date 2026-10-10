#pragma once

#include "Framework/Render/Public/RenderPass.h"

namespace ShadowEngine
{
    class DebugRenderPass final : public RenderPass
    {
        public:
            bool Execute(const RenderQueue& Queue, const RenderPassContext& Context) override;
    };
}
