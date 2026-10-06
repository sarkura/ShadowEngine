#include "Framework/Scene/Public/Scene.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>

#include <cstring>

namespace ShadowEngine
{
    namespace
    {
        void WriteNormalMatrix(const float* World, float* OutMatrix)
        {
            const glm::mat4 WorldMatrix = glm::make_mat4(World);
            const glm::mat3 Linear = glm::mat3(WorldMatrix);
            const glm::mat3 Normal = glm::transpose(glm::inverse(Linear));
            glm::mat4 Packed(1.0F);
            Packed[0] = glm::vec4(Normal[0], 0.0F);
            Packed[1] = glm::vec4(Normal[1], 0.0F);
            Packed[2] = glm::vec4(Normal[2], 0.0F);
            std::memcpy(OutMatrix, glm::value_ptr(Packed), sizeof(float) * 16);
        }
    }

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

    void Scene::WriteRenderProxy(RenderProxy& Out) const
    {
        Out.Meshes.clear();
        Out.Meshes.reserve(Entities.size());
        for (const Entity& Item : Entities)
        {
            MeshRenderProxy Proxy;
            Proxy.Mesh = Item.GetMesh();
            Item.GetTransform().WriteWorldMatrix(Proxy.World);
            WriteNormalMatrix(Proxy.World, Proxy.NormalMatrix);
            Out.Meshes.push_back(Proxy);
        }

        Out.DirectLights.clear();
        Out.DirectLights.reserve(DirectLights.size());
        for (const DirectLight& Light : DirectLights)
        {
            DirectLightRenderProxy Proxy;
            Light.Write(Proxy);
            Out.DirectLights.push_back(Proxy);
        }
    }

    const std::deque<Entity>& Scene::GetEntities() const
    {
        return Entities;
    }
}
