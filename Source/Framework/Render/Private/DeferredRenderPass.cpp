#include "Framework/Render/Public/DeferredRenderPass.h"

namespace ShadowEngine
{
    bool DeferredRenderPass::Execute(const RenderQueue& Queue, const RenderPassContext& Context)
    {
        (void)Queue;
        (void)Context;
        return false;
    }
}
