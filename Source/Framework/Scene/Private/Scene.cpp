#include "Framework/Scene/Public/Scene.h"

namespace ShadowEngine
{
    Entity& Scene::CreateEntity()
    {
        Entities.emplace_back();
        return Entities.back();
    }

    bool Scene::CreateDirectLight()
    {
        if (bHasDirectLight)
        {
            return false;
        }

        DirectLight = {};
        bHasDirectLight = true;
        return true;
    }

    PointLight& Scene::CreatePointLight()
    {
        PointLights.emplace_back();
        return PointLights.back();
    }

    bool Scene::CreateSkyLight()
    {
        if (bHasSkyLight)
        {
            return false;
        }

        SkyLight = {};
        bHasSkyLight = true;
        return true;
    }

    const std::deque<Entity>& Scene::GetEntities() const
    {
        return Entities;
    }

    bool Scene::HasDirectLight() const
    {
        return bHasDirectLight;
    }

    const DirectLight& Scene::GetDirectLight() const
    {
        return DirectLight;
    }

    DirectLight& Scene::GetDirectLight()
    {
        return DirectLight;
    }

    const std::deque<PointLight>& Scene::GetPointLights() const
    {
        return PointLights;
    }

    bool Scene::HasSkyLight() const
    {
        return bHasSkyLight;
    }

    const SkyLight& Scene::GetSkyLight() const
    {
        return SkyLight;
    }

    SkyLight& Scene::GetSkyLight()
    {
        return SkyLight;
    }
}
