#include "Framework/Render/Public/DeferredRenderer.h"

namespace ShadowEngine
{
    bool DeferredRenderer::RenderFrame()
    {
        return false;
    }

    const char* DeferredRenderer::GetName() const
    {
        return "DeferredRenderer";
    }
}
