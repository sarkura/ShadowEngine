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

        struct MovementSetting
        {
            float ForwardSpeed = 0.0F;
            float RightSpeed = 0.0F;
            float UpSpeed = 0.0F;
            float DownSpeed = 0.0F;
            float YawSpeed = 0.0F;
            float PitchSpeed = 0.0F;
        };
    }

    namespace RenderSetting
    {
        struct RHISetting
        {
            std::string Renderer;
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

        enum class LightType
        {
            Direct,
            Point,
            Sky
        };

        struct Light
        {
            LightType Type = LightType::Direct;
            std::vector<float> Direction;
            std::vector<float> Position;
            std::vector<float> Color;
            float Intensity = 0.0F;
            float Radius = 0.0F;
        };

        struct Setting
        {
            std::vector<Mesh> Meshes;
            std::vector<Light> Lights;
        };
    }

    namespace GlobalTextureSetting
    {
        struct Setting
        {
            std::string EnvironmentMap;
        };
    }
}
