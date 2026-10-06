#include "Framework/Render/Public/Camera.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/trigonometric.hpp>

namespace ShadowEngine
{
    namespace
    {
        constexpr float MinPitch = -1.5533F;
        constexpr float MaxPitch = 1.5533F;
        constexpr glm::vec3 WorldUp(0.0F, 1.0F, 0.0F);
    }

    Camera::Camera()
    {
        const glm::vec3 Direction = glm::normalize(-Position);
        Pitch = glm::asin(Direction.y);
        Yaw = glm::atan(Direction.x, Direction.z);
    }

    glm::vec3 Camera::ForwardDirection() const
    {
        return glm::vec3(
            glm::cos(Pitch) * glm::sin(Yaw),
            glm::sin(Pitch),
            glm::cos(Pitch) * glm::cos(Yaw));
    }

    void Camera::SetMoveSpeed(float InForwardSpeed, float InRightSpeed, float InUpSpeed, float InDownSpeed)
    {
        ForwardSpeed = InForwardSpeed;
        RightSpeed = InRightSpeed;
        UpSpeed = InUpSpeed;
        DownSpeed = InDownSpeed;
    }

    void Camera::AddLook(float YawDelta, float PitchDelta)
    {
        Yaw += YawDelta;
        Pitch = glm::clamp(Pitch + PitchDelta, MinPitch, MaxPitch);
    }

    void Camera::Move(float DeltaTime, float Forward, float Right, float Up)
    {
        if (DeltaTime <= 0.0F)
        {
            return;
        }

        const glm::vec3 Face = ForwardDirection();
        const glm::vec3 Strafe = glm::normalize(glm::cross(Face, WorldUp));
        const float VerticalSpeed = Up >= 0.0F ? UpSpeed : DownSpeed;
        Position += Face * Forward * ForwardSpeed * DeltaTime;
        Position += Strafe * Right * RightSpeed * DeltaTime;
        Position += WorldUp * Up * VerticalSpeed * DeltaTime;
    }

    glm::mat4 Camera::ViewMatrix() const
    {
        const glm::vec3 Face = ForwardDirection();
        return glm::lookAtRH(Position, Position + Face, WorldUp);
    }

    glm::vec3 Camera::GetPosition() const
    {
        return Position;
    }
}
