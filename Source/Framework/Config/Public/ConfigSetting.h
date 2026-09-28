#pragma once
#include <vector>
#include <string>

namespace ShadowEngine
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