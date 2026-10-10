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
            PointLight& CreatePointLight();
            SkyLight& CreateSkyLight();

            [[nodiscard]] const std::deque<Entity>& GetEntities() const;
            [[nodiscard]] const std::deque<DirectLight>& GetDirectLights() const;
            [[nodiscard]] const std::deque<PointLight>& GetPointLights() const;
            [[nodiscard]] const std::deque<SkyLight>& GetSkyLights() const;

        private:
            std::deque<Entity> Entities;
            std::deque<DirectLight> DirectLights;
            std::deque<PointLight> PointLights;
            std::deque<SkyLight> SkyLights;
    };
}
