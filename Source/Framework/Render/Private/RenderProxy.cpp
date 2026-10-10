#include "Framework/Render/Public/RenderProxy.h"

#include "Framework/Scene/Public/Scene.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>

#include <cstring>
#include <utility>

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

    void SyncRenderProxy(const Scene& InScene, RenderProxy& Out)
    {
        Out.Meshes.clear();
        Out.Meshes.reserve(InScene.GetEntities().size());
        for (const Entity& Item : InScene.GetEntities())
        {
            MeshRenderProxy Mesh;
            Mesh.Mesh = Item.GetMesh();
            Mesh.Materials = Item.GetMaterials();
            Item.GetTransform().WriteWorldMatrix(Mesh.World);
            WriteNormalMatrix(Mesh.World, Mesh.NormalMatrix);
            Out.Meshes.push_back(std::move(Mesh));
        }

        Out.DirectLights.clear();
        Out.DirectLights.reserve(InScene.GetDirectLights().size());
        for (const DirectLight& Item : InScene.GetDirectLights())
        {
            DirectLightRenderProxy Light;
            Item.GetDirection(Light.Direction);
            Item.GetColor(Light.Color);
            Light.Intensity = Item.GetIntensity();
            Out.DirectLights.push_back(Light);
        }

        Out.PointLights.clear();
        Out.PointLights.reserve(InScene.GetPointLights().size());
        for (const PointLight& Item : InScene.GetPointLights())
        {
            PointLightRenderProxy Light;
            Item.GetPosition(Light.Position);
            Item.GetColor(Light.Color);
            Light.Radius = Item.GetRadius();
            Light.Intensity = Item.GetIntensity();
            Out.PointLights.push_back(Light);
        }

        Out.SkyLights.clear();
        Out.SkyLights.reserve(InScene.GetSkyLights().size());
        for (const SkyLight& Item : InScene.GetSkyLights())
        {
            SkyLightRenderProxy Light;
            Item.GetColor(Light.Color);
            Light.Intensity = Item.GetIntensity();
            Out.SkyLights.push_back(Light);
        }
    }
}
