#pragma once

#include "ThirdPartyAdapter/Mathematics/Public/GLMAdapter.h"

#include "Framework/Render/Public/RenderPass.h"


namespace ShadowEngine
{
    void ApplyProjectionJitter(Mat4& Projection, float JitterX, float JitterY, float Width, float Height);
    bool DrawSkyBackground(const RenderPassContext& Context);
}
