#pragma once

#include "Framework/Asset/Public/AssetHandle.h"
#include "Framework/Material/Public/MaterialHandle.h"

#include <vector>

namespace ShadowEngine
{
    class Scene;

    struct MeshRenderProxy
    {
        MeshHandle Mesh;
        float World[16] = {};
        float NormalMatrix[16] = {};
        std::vector<MaterialInstanceHandle> Materials;
    };

    struct DirectLightRenderProxy
    {
        float Direction[3] = {0.0F, -1.0F, 0.0F};
        float Color[3] = {1.0F, 1.0F, 1.0F};
        float Intensity = 1.0F;
    };

    struct RenderProxy
    {
        std::vector<MeshRenderProxy> Meshes;
        std::vector<DirectLightRenderProxy> DirectLights;
    };

    void SyncRenderProxy(const Scene& InScene, RenderProxy& Out);
}
