#pragma once

#include <vector>

namespace ShadowEngine
{
    class MeshAsset;

    struct MeshRenderProxy
    {
        const MeshAsset* Mesh = nullptr;
        float World[16] = {};
        float NormalMatrix[16] = {};
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
}
