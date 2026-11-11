#pragma once

#include "Mathematics/Public/Math.h"

namespace ShadowEngine
{
    class GLMAdapter final
    {
        public:
            static float Radians(float Degrees);
            static float Sin(float Radians);
            static float Cos(float Radians);
            static float Asin(float Value);
            static float Atan(float Y, float X);
            static float Clamp(float Value, float Minimum, float Maximum);

            static Vec3 Normalize(const Vec3& Value);
            static float Length(const Vec3& Value);
            static Vec3 Cross(const Vec3& Left, const Vec3& Right);
            static float Dot(const Vec3& Left, const Vec3& Right);
            static Vec3 Min(const Vec3& Left, const Vec3& Right);
            static Vec3 Max(const Vec3& Left, const Vec3& Right);

            static Mat4 MakeMat4(const float* Values);
            static Mat4 Inverse(const Mat4& Value);
            static Mat3 Inverse(const Mat3& Value);
            static Mat3 Transpose(const Mat3& Value);
            static const float* ValuePtr(const Mat4& Value);

            static Mat4 PerspectiveRH_ZO(float FieldOfViewY, float Aspect, float NearPlane, float FarPlane);
            static Mat4 OrthoRH_ZO(float Left, float Right, float Bottom, float Top, float NearPlane, float FarPlane);
            static Mat4 LookAtRH(const Vec3& Eye, const Vec3& Center, const Vec3& Up);
            static Mat4 Translate(const Mat4& Value, const Vec3& Offset);
            static Mat4 Rotate(const Mat4& Value, float Angle, const Vec3& Axis);
            static Mat4 Scale(const Mat4& Value, const Vec3& Factors);
    };
}
