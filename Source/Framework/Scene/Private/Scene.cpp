#include "Framework/Scene/Public/Scene.h"

namespace ShadowEngine
{
    Entity& Scene::CreateEntity()
    {
        Entities.emplace_back();
        return Entities.back();
    }

    DirectLight& Scene::CreateDirectLight()
    {
        DirectLights.emplace_back();
        return DirectLights.back();
    }

    PointLight& Scene::CreatePointLight()
    {
        PointLights.emplace_back();
        return PointLights.back();
    }

    SkyLight& Scene::CreateSkyLight()
    {
        SkyLights.emplace_back();
        return SkyLights.back();
    }

    const std::deque<Entity>& Scene::GetEntities() const
    {
        return Entities;
    }

    const std::deque<DirectLight>& Scene::GetDirectLights() const
    {
        return DirectLights;
    }

    const std::deque<PointLight>& Scene::GetPointLights() const
    {
        return PointLights;
    }

    const std::deque<SkyLight>& Scene::GetSkyLights() const
    {
        return SkyLights;
    }
}
