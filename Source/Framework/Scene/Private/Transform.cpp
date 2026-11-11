#include "ThirdPartyAdapter/Mathematics/Public/GLMAdapter.h"
#include "Framework/Scene/Public/Transform.h"


#include <cstring>

namespace ShadowEngine
{
    void Transform::SetTranslation(float X, float Y, float Z)
    {
        TranslationX = X;
        TranslationY = Y;
        TranslationZ = Z;
    }

    void Transform::SetRotation(float InPitch, float InYaw, float InRoll)
    {
        Pitch = InPitch;
        Yaw = InYaw;
        Roll = InRoll;
    }

    void Transform::SetScale(float X, float Y, float Z)
    {
        ScaleX = X;
        ScaleY = Y;
        ScaleZ = Z;
    }

    void Transform::WriteWorldMatrix(float* OutMatrix) const
    {
        const Mat4 Matrix =
            GLMAdapter::Translate(Mat4(1.0F), Vec3(TranslationX, TranslationY, TranslationZ)) *
            GLMAdapter::Rotate(Mat4(1.0F), Yaw, Vec3(0.0F, 1.0F, 0.0F)) *
            GLMAdapter::Rotate(Mat4(1.0F), Pitch, Vec3(1.0F, 0.0F, 0.0F)) *
            GLMAdapter::Rotate(Mat4(1.0F), Roll, Vec3(0.0F, 0.0F, 1.0F)) *
            GLMAdapter::Scale(Mat4(1.0F), Vec3(ScaleX, ScaleY, ScaleZ));
        std::memcpy(OutMatrix, GLMAdapter::ValuePtr(Matrix), sizeof(float) * 16);
    }
}
