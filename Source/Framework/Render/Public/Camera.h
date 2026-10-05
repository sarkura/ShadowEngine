#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace ShadowEngine
{
    class Camera
    {
        public:
            Camera();

            void SetMoveSpeed(float ForwardSpeed, float RightSpeed, float UpSpeed, float DownSpeed);
            void AddLook(float YawDelta, float PitchDelta);
            void Move(float DeltaTime, float Forward, float Right, float Up);
            [[nodiscard]] glm::mat4 ViewMatrix() const;

        private:
            [[nodiscard]] glm::vec3 ForwardDirection() const;

            glm::vec3 Position{0.0F, 4.0F, 14.0F};
            float Yaw = 0.0F;
            float Pitch = 0.0F;
            float ForwardSpeed = 10.0F;
            float RightSpeed = 10.0F;
            float UpSpeed = 10.0F;
            float DownSpeed = 10.0F;
    };
}
