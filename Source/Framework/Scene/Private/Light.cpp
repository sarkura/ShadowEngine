#include "Framework/Scene/Public/Light.h"

#include <cmath>

namespace ShadowEngine
{
    void DirectLight::SetDirection(float X, float Y, float Z)
    {
        const float Length = std::sqrt(X * X + Y * Y + Z * Z);
        if (Length <= 0.0001F)
        {
            DirectionX = 0.0F;
            DirectionY = -1.0F;
            DirectionZ = 0.0F;
            return;
        }

        DirectionX = X / Length;
        DirectionY = Y / Length;
        DirectionZ = Z / Length;
    }

    void DirectLight::SetColor(float R, float G, float B)
    {
        ColorR = R;
        ColorG = G;
        ColorB = B;
    }

    void DirectLight::SetIntensity(float InIntensity)
    {
        Intensity = InIntensity;
    }

    void DirectLight::GetDirection(float Out[3]) const
    {
        Out[0] = DirectionX;
        Out[1] = DirectionY;
        Out[2] = DirectionZ;
    }

    void DirectLight::GetColor(float Out[3]) const
    {
        Out[0] = ColorR;
        Out[1] = ColorG;
        Out[2] = ColorB;
    }

    float DirectLight::GetIntensity() const
    {
        return Intensity;
    }
}
