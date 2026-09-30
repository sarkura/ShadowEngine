#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/RHI/Public/RHITypes.h"

namespace ShadowEngine
{
    class RHIPipeline;
    class RHITexture;

    class RHICommandList : public NonCopyable
    {
        public:
            virtual ~RHICommandList() = default;

            virtual void Begin() = 0;
            virtual void End() = 0;

            virtual void BeginRenderPass(RHITexture& RenderTarget, const RHIColor& ClearColor) = 0;
            virtual void EndRenderPass() = 0;

            virtual void SetPipeline(RHIPipeline& Pipeline) = 0;
            virtual void SetViewport(const RHIViewport& Viewport) = 0;
            virtual void SetScissor(const RHIRect& Scissor) = 0;
            virtual void Draw(uint32 VertexCount, uint32 FirstVertex = 0) = 0;
    };
}
