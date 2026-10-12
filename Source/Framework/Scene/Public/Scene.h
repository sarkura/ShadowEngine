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
            [[nodiscard]] bool CreateDirectLight();
            PointLight& CreatePointLight();
            [[nodiscard]] bool CreateSkyLight();

            [[nodiscard]] const std::deque<Entity>& GetEntities() const;
            [[nodiscard]] bool HasDirectLight() const;
            [[nodiscard]] const DirectLight& GetDirectLight() const;
            [[nodiscard]] DirectLight& GetDirectLight();
            [[nodiscard]] const std::deque<PointLight>& GetPointLights() const;
            [[nodiscard]] bool HasSkyLight() const;
            [[nodiscard]] const SkyLight& GetSkyLight() const;
            [[nodiscard]] SkyLight& GetSkyLight();

        private:
            std::deque<Entity> Entities;
            DirectLight DirectLight;
            bool bHasDirectLight = false;
            std::deque<PointLight> PointLights;
            SkyLight SkyLight;
            bool bHasSkyLight = false;
    };
}
