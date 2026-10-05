#include "Framework/Scene/Public/Scene.h"

namespace ShadowEngine
{
    Entity& Scene::CreateEntity()
    {
        Entities.emplace_back();
        return Entities.back();
    }

    const std::deque<Entity>& Scene::GetEntities() const
    {
        return Entities;
    }
}
