#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Render/Public/RenderProxy.h"
#include "Framework/Scene/Public/Entity.h"
#include "Framework/Scene/Public/Light.h"

#include <deque>
#include <vector>

namespace ShadowEngine
{
    class Scene final : public NonCopyable
    {
        public:
            Entity& CreateEntity();
            DirectLight& CreateDirectLight();

            void WriteRenderProxy(RenderProxy& Out) const;

            [[nodiscard]] const std::deque<Entity>& GetEntities() const;

        private:
            std::deque<Entity> Entities;
            std::deque<DirectLight> DirectLights;
    };
}
