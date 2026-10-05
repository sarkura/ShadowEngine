#pragma once

namespace ShadowEngine
{
    class Transform final
    {
        public:
            void SetTranslation(float X, float Y, float Z);
            void SetRotation(float Pitch, float Yaw, float Roll);
            void SetScale(float X, float Y, float Z);

            void WriteWorldMatrix(float* OutMatrix) const;

        private:
            float TranslationX = 0.0F;
            float TranslationY = 0.0F;
            float TranslationZ = 0.0F;
            float Pitch = 0.0F;
            float Yaw = 0.0F;
            float Roll = 0.0F;
            float ScaleX = 1.0F;
            float ScaleY = 1.0F;
            float ScaleZ = 1.0F;
    };
}
