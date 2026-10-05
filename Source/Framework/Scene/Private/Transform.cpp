#include "Framework/Scene/Public/Transform.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

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
        const glm::mat4 Matrix =
            glm::translate(glm::mat4(1.0F), glm::vec3(TranslationX, TranslationY, TranslationZ)) *
            glm::rotate(glm::mat4(1.0F), Yaw, glm::vec3(0.0F, 1.0F, 0.0F)) *
            glm::rotate(glm::mat4(1.0F), Pitch, glm::vec3(1.0F, 0.0F, 0.0F)) *
            glm::rotate(glm::mat4(1.0F), Roll, glm::vec3(0.0F, 0.0F, 1.0F)) *
            glm::scale(glm::mat4(1.0F), glm::vec3(ScaleX, ScaleY, ScaleZ));
        std::memcpy(OutMatrix, glm::value_ptr(Matrix), sizeof(float) * 16);
    }
}
