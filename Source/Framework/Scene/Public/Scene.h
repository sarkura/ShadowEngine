#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Scene/Public/Entity.h"
#include "Framework/Scene/Public/Light.h"

#include <deque>

namespace ShadowEngine
{
    class Scene final : public NonCopyable
    {
        public:
            Entity& CreateEntity();
            DirectLight& CreateDirectLight();

            [[nodiscard]] const std::deque<Entity>& GetEntities() const;
            [[nodiscard]] const std::deque<DirectLight>& GetDirectLights() const;

        private:
            std::deque<Entity> Entities;
            std::deque<DirectLight> DirectLights;
    };
}
