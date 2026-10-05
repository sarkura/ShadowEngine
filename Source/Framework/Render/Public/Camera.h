#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace ShadowEngine
{
    class Camera
    {
        public:
            Camera();

            void AddLook(float YawDelta, float PitchDelta);
            void Move(float DeltaTime, float Forward, float Right);
            [[nodiscard]] glm::mat4 ViewMatrix() const;

        private:
            [[nodiscard]] glm::vec3 ForwardDirection() const;

            glm::vec3 Position{0.0F, 4.0F, 14.0F};
            float Yaw = 0.0F;
            float Pitch = 0.0F;
    };
}
