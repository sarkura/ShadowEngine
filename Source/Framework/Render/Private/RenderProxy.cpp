#include "ThirdPartyAdapter/Mathematics/Public/GLMAdapter.h"
#include "Framework/Render/Public/RenderProxy.h"

#include "Framework/Scene/Public/Scene.h"


#include <cstring>
#include <utility>

namespace ShadowEngine
{
    namespace
    {
        void WriteNormalMatrix(const float* World, float* OutMatrix)
        {
            const Mat4 WorldMatrix = GLMAdapter::MakeMat4(World);
            const Mat3 Linear = Mat3(WorldMatrix);
            const Mat3 Normal = GLMAdapter::Transpose(GLMAdapter::Inverse(Linear));
            Mat4 Packed(1.0F);
            Packed[0] = Vec4(Normal[0], 0.0F);
            Packed[1] = Vec4(Normal[1], 0.0F);
            Packed[2] = Vec4(Normal[2], 0.0F);
            std::memcpy(OutMatrix, GLMAdapter::ValuePtr(Packed), sizeof(float) * 16);
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

        Out.bHasDirectLight = InScene.HasDirectLight();
        Out.DirectLight = {};
        if (Out.bHasDirectLight)
        {
            const DirectLight& Item = InScene.GetDirectLight();
            Item.GetDirection(Out.DirectLight.Direction);
            Item.GetColor(Out.DirectLight.Color);
            Out.DirectLight.Intensity = Item.GetIntensity();
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

        Out.bHasSkyLight = InScene.HasSkyLight();
        Out.SkyLight = {};
        if (Out.bHasSkyLight)
        {
            const SkyLight& Item = InScene.GetSkyLight();
            Item.GetColor(Out.SkyLight.Color);
            Out.SkyLight.Intensity = Item.GetIntensity();
        }
    }
}
