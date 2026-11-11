#include "ThirdPartyAdapter/Mathematics/Public/GLMAdapter.h"

#include <cstring>

#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace ShadowEngine
{
    namespace
    {
        glm::vec3 ToGlm(const Vec3& Value)
        {
            return glm::vec3(Value.x, Value.y, Value.z);
        }

        Vec3 FromGlm(const glm::vec3& Value)
        {
            return Vec3(Value.x, Value.y, Value.z);
        }

        glm::mat3 ToGlm(const Mat3& Value)
        {
            return glm::mat3(ToGlm(Value[0]), ToGlm(Value[1]), ToGlm(Value[2]));
        }

        Mat3 FromGlm(const glm::mat3& Value)
        {
            Mat4 Packed(0.0F);
            Packed[0] = Vec4(FromGlm(Value[0]), 0.0F);
            Packed[1] = Vec4(FromGlm(Value[1]), 0.0F);
            Packed[2] = Vec4(FromGlm(Value[2]), 0.0F);
            return Mat3(Packed);
        }

        glm::mat4 ToGlm(const Mat4& Value)
        {
            glm::mat4 Result(1.0F);
            std::memcpy(glm::value_ptr(Result), Value.Data(), sizeof(float) * 16);
            return Result;
        }

        Mat4 FromGlm(const glm::mat4& Value)
        {
            Mat4 Result(0.0F);
            std::memcpy(Result.Data(), glm::value_ptr(Value), sizeof(float) * 16);
            return Result;
        }
    }

    float GLMAdapter::Radians(float Degrees)
    {
        return glm::radians(Degrees);
    }

    float GLMAdapter::Sin(float Radians)
    {
        return glm::sin(Radians);
    }

    float GLMAdapter::Cos(float Radians)
    {
        return glm::cos(Radians);
    }

    float GLMAdapter::Asin(float Value)
    {
        return glm::asin(Value);
    }

    float GLMAdapter::Atan(float Y, float X)
    {
        return glm::atan(Y, X);
    }

    float GLMAdapter::Clamp(float Value, float Minimum, float Maximum)
    {
        return glm::clamp(Value, Minimum, Maximum);
    }

    Vec3 GLMAdapter::Normalize(const Vec3& Value)
    {
        return FromGlm(glm::normalize(ToGlm(Value)));
    }

    float GLMAdapter::Length(const Vec3& Value)
    {
        return glm::length(ToGlm(Value));
    }

    Vec3 GLMAdapter::Cross(const Vec3& Left, const Vec3& Right)
    {
        return FromGlm(glm::cross(ToGlm(Left), ToGlm(Right)));
    }

    float GLMAdapter::Dot(const Vec3& Left, const Vec3& Right)
    {
        return glm::dot(ToGlm(Left), ToGlm(Right));
    }

    Vec3 GLMAdapter::Min(const Vec3& Left, const Vec3& Right)
    {
        return FromGlm(glm::min(ToGlm(Left), ToGlm(Right)));
    }

    Vec3 GLMAdapter::Max(const Vec3& Left, const Vec3& Right)
    {
        return FromGlm(glm::max(ToGlm(Left), ToGlm(Right)));
    }

    Mat4 GLMAdapter::MakeMat4(const float* Values)
    {
        return FromGlm(glm::make_mat4(Values));
    }

    Mat4 GLMAdapter::Inverse(const Mat4& Value)
    {
        return FromGlm(glm::inverse(ToGlm(Value)));
    }

    Mat3 GLMAdapter::Inverse(const Mat3& Value)
    {
        return FromGlm(glm::inverse(ToGlm(Value)));
    }

    Mat3 GLMAdapter::Transpose(const Mat3& Value)
    {
        return FromGlm(glm::transpose(ToGlm(Value)));
    }

    const float* GLMAdapter::ValuePtr(const Mat4& Value)
    {
        return Value.Data();
    }

    Mat4 GLMAdapter::PerspectiveRH_ZO(float FieldOfViewY, float Aspect, float NearPlane, float FarPlane)
    {
        return FromGlm(glm::perspectiveRH_ZO(FieldOfViewY, Aspect, NearPlane, FarPlane));
    }

    Mat4 GLMAdapter::OrthoRH_ZO(float Left, float Right, float Bottom, float Top, float NearPlane, float FarPlane)
    {
        return FromGlm(glm::orthoRH_ZO(Left, Right, Bottom, Top, NearPlane, FarPlane));
    }

    Mat4 GLMAdapter::LookAtRH(const Vec3& Eye, const Vec3& Center, const Vec3& Up)
    {
        return FromGlm(glm::lookAtRH(ToGlm(Eye), ToGlm(Center), ToGlm(Up)));
    }

    Mat4 GLMAdapter::Translate(const Mat4& Value, const Vec3& Offset)
    {
        return FromGlm(glm::translate(ToGlm(Value), ToGlm(Offset)));
    }

    Mat4 GLMAdapter::Rotate(const Mat4& Value, float Angle, const Vec3& Axis)
    {
        return FromGlm(glm::rotate(ToGlm(Value), Angle, ToGlm(Axis)));
    }

    Mat4 GLMAdapter::Scale(const Mat4& Value, const Vec3& Factors)
    {
        return FromGlm(glm::scale(ToGlm(Value), ToGlm(Factors)));
    }
}
