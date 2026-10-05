#pragma once
#include <vector>
#include <string>

namespace ShadowEngine
{
    namespace EngineSetting
    {
        struct ViewportSetting
        {
            int Width = 0;
            int Height = 0;
            int AspectWidth = 0;
            int AspectHeight = 0;
            float FOV = 0.0F;
            float NearPlane = 0.0F;
            float FarPlane = 0.0F;
            std::vector<float> BackgroundColor;
            std::string ClearFlags;
        };
    }

    namespace RenderSetting
    {
        struct RHISetting
        {
            std::string Backend;
            bool bVSync = true;
            bool bDebugLayer = false;
            int BackBufferCount = 2;
        };
    }

    namespace SceneSetting
    {
        struct Transform
        {
            std::vector<float> Translation;
            std::vector<float> Rotation;
            std::vector<float> Scale;
        };

        struct MeshInstance
        {
            Transform Transform;
        };

        struct Mesh
        {
            std::string Path;
            std::vector<MeshInstance> Instances;
        };

        struct Light
        {
            std::vector<float> Position;
        };

        struct Setting
        {
            std::vector<Mesh> Meshes;
            std::vector<Light> Lights;
        };
    }
}
