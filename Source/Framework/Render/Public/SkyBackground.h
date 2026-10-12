#pragma once

#include "Framework/Render/Public/RenderPass.h"

#include <glm/mat4x4.hpp>

namespace ShadowEngine
{
    void ApplyProjectionJitter(glm::mat4& Projection, float JitterX, float JitterY, float Width, float Height);
    bool DrawSkyBackground(const RenderPassContext& Context);
}
