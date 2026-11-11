#include "ThirdPartyAdapter/Mathematics/Public/GLMAdapter.h"
#include "Framework/Render/Public/Camera.h"


namespace ShadowEngine
{
    namespace
    {
        constexpr float MinPitch = -1.5533F;
        constexpr float MaxPitch = 1.5533F;
        constexpr Vec3 WorldUp(0.0F, 1.0F, 0.0F);
    }

    Camera::Camera()
    {
        const Vec3 Direction = GLMAdapter::Normalize(-Position);
        Pitch = GLMAdapter::Asin(Direction.y);
        Yaw = GLMAdapter::Atan(Direction.x, Direction.z);
    }

    Vec3 Camera::ForwardDirection() const
    {
        return Vec3(
            GLMAdapter::Cos(Pitch) * GLMAdapter::Sin(Yaw),
            GLMAdapter::Sin(Pitch),
            GLMAdapter::Cos(Pitch) * GLMAdapter::Cos(Yaw));
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
        Pitch = GLMAdapter::Clamp(Pitch + PitchDelta, MinPitch, MaxPitch);
    }

    void Camera::Move(float DeltaTime, float Forward, float Right, float Up)
    {
        if (DeltaTime <= 0.0F)
        {
            return;
        }

        const Vec3 Face = ForwardDirection();
        const Vec3 Strafe = GLMAdapter::Normalize(GLMAdapter::Cross(Face, WorldUp));
        const float VerticalSpeed = Up >= 0.0F ? UpSpeed : DownSpeed;
        Position += Face * Forward * ForwardSpeed * DeltaTime;
        Position += Strafe * Right * RightSpeed * DeltaTime;
        Position += WorldUp * Up * VerticalSpeed * DeltaTime;
    }

    Mat4 Camera::ViewMatrix() const
    {
        const Vec3 Face = ForwardDirection();
        return GLMAdapter::LookAtRH(Position, Position + Face, WorldUp);
    }

    Vec3 Camera::GetPosition() const
    {
        return Position;
    }
}
