#pragma once

#include "Framework/Render/Public/RenderProxy.h"

namespace ShadowEngine
{
    class DirectLight final
    {
        public:
            void SetDirection(float X, float Y, float Z);
            void SetColor(float R, float G, float B);
            void SetIntensity(float Intensity);

            void Write(DirectLightRenderProxy& Out) const;

        private:
            float DirectionX = 0.0F;
            float DirectionY = -1.0F;
            float DirectionZ = 0.0F;
            float ColorR = 1.0F;
            float ColorG = 1.0F;
            float ColorB = 1.0F;
            float Intensity = 1.0F;
    };
}
