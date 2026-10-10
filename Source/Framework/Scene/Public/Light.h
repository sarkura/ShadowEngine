#pragma once

namespace ShadowEngine
{
    class DirectLight final
    {
        public:
            void SetDirection(float X, float Y, float Z);
            void SetColor(float R, float G, float B);
            void SetIntensity(float Intensity);

            void GetDirection(float Out[3]) const;
            void GetColor(float Out[3]) const;
            [[nodiscard]] float GetIntensity() const;

        private:
            float DirectionX = 0.0F;
            float DirectionY = -1.0F;
            float DirectionZ = 0.0F;
            float ColorR = 1.0F;
            float ColorG = 1.0F;
            float ColorB = 1.0F;
            float Intensity = 1.0F;
    };

    class PointLight final
    {
        public:
            void SetPosition(float X, float Y, float Z);
            void SetColor(float R, float G, float B);
            void SetIntensity(float Intensity);
            void SetRadius(float Radius);

            void GetPosition(float Out[3]) const;
            void GetColor(float Out[3]) const;
            [[nodiscard]] float GetIntensity() const;
            [[nodiscard]] float GetRadius() const;

        private:
            float PositionX = 0.0F;
            float PositionY = 2.0F;
            float PositionZ = 0.0F;
            float ColorR = 1.0F;
            float ColorG = 1.0F;
            float ColorB = 1.0F;
            float Intensity = 1.0F;
            float Radius = 8.0F;
    };

    class SkyLight final
    {
        public:
            void SetColor(float R, float G, float B);
            void SetIntensity(float Intensity);

            void GetColor(float Out[3]) const;
            [[nodiscard]] float GetIntensity() const;

        private:
            float ColorR = 0.5F;
            float ColorG = 0.6F;
            float ColorB = 0.8F;
            float Intensity = 0.2F;
    };
}
