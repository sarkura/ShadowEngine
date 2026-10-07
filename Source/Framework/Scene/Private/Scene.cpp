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

    const std::deque<Entity>& Scene::GetEntities() const
    {
        return Entities;
    }

    const std::deque<DirectLight>& Scene::GetDirectLights() const
    {
        return DirectLights;
    }
}
