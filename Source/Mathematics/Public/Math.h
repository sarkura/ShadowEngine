#pragma once

namespace ShadowEngine
{
    struct Vec4;

    struct Vec3
    {
        float x = 0.0F;
        float y = 0.0F;
        float z = 0.0F;

        constexpr Vec3() = default;
        constexpr Vec3(float X, float Y, float Z)
            : x(X)
            , y(Y)
            , z(Z)
        {
        }

        constexpr explicit Vec3(float Value)
            : x(Value)
            , y(Value)
            , z(Value)
        {
        }

        constexpr explicit Vec3(const Vec4& Value);

        [[nodiscard]] constexpr Vec3 operator-() const
        {
            return Vec3(-x, -y, -z);
        }

        [[nodiscard]] constexpr Vec3 operator+(const Vec3& Right) const
        {
            return Vec3(x + Right.x, y + Right.y, z + Right.z);
        }

        [[nodiscard]] constexpr Vec3 operator-(const Vec3& Right) const
        {
            return Vec3(x - Right.x, y - Right.y, z - Right.z);
        }

        [[nodiscard]] constexpr Vec3 operator*(float Scale) const
        {
            return Vec3(x * Scale, y * Scale, z * Scale);
        }

        [[nodiscard]] constexpr Vec3 operator/(float Scale) const
        {
            return Vec3(x / Scale, y / Scale, z / Scale);
        }

        constexpr Vec3& operator+=(const Vec3& Right)
        {
            x += Right.x;
            y += Right.y;
            z += Right.z;
            return *this;
        }
    };

    struct Vec4
    {
        float x = 0.0F;
        float y = 0.0F;
        float z = 0.0F;
        float w = 0.0F;

        constexpr Vec4() = default;
        constexpr Vec4(float X, float Y, float Z, float W)
            : x(X)
            , y(Y)
            , z(Z)
            , w(W)
        {
        }

        constexpr Vec4(const Vec3& Value, float W)
            : x(Value.x)
            , y(Value.y)
            , z(Value.z)
            , w(W)
        {
        }

        constexpr explicit Vec4(float Value)
            : x(Value)
            , y(Value)
            , z(Value)
            , w(Value)
        {
        }

        [[nodiscard]] constexpr float& operator[](int Index)
        {
            switch (Index)
            {
                case 0:
                    return x;
                case 1:
                    return y;
                case 2:
                    return z;
                default:
                    return w;
            }
        }

        [[nodiscard]] constexpr const float& operator[](int Index) const
        {
            switch (Index)
            {
                case 0:
                    return x;
                case 1:
                    return y;
                case 2:
                    return z;
                default:
                    return w;
            }
        }

        [[nodiscard]] constexpr Vec4 operator+(const Vec4& Right) const
        {
            return Vec4(x + Right.x, y + Right.y, z + Right.z, w + Right.w);
        }

        [[nodiscard]] constexpr Vec4 operator-(const Vec4& Right) const
        {
            return Vec4(x - Right.x, y - Right.y, z - Right.z, w - Right.w);
        }

        [[nodiscard]] constexpr Vec4 operator*(float Scale) const
        {
            return Vec4(x * Scale, y * Scale, z * Scale, w * Scale);
        }

        [[nodiscard]] constexpr Vec4 operator/(float Scale) const
        {
            return Vec4(x / Scale, y / Scale, z / Scale, w / Scale);
        }
    };

    constexpr Vec3::Vec3(const Vec4& Value)
        : x(Value.x)
        , y(Value.y)
        , z(Value.z)
    {
    }

    struct Mat4;

    struct Mat3
    {
        Vec3 Columns[3] = {Vec3(1.0F, 0.0F, 0.0F), Vec3(0.0F, 1.0F, 0.0F), Vec3(0.0F, 0.0F, 1.0F)};

        constexpr Mat3() = default;
        constexpr explicit Mat3(const Mat4& Value);

        [[nodiscard]] constexpr Vec3& operator[](int Column)
        {
            return Columns[Column];
        }

        [[nodiscard]] constexpr const Vec3& operator[](int Column) const
        {
            return Columns[Column];
        }

        [[nodiscard]] constexpr Vec3 operator*(const Vec3& Value) const
        {
            return Columns[0] * Value.x + Columns[1] * Value.y + Columns[2] * Value.z;
        }
    };

    struct Mat4
    {
        Vec4 Columns[4] = {
            Vec4(1.0F, 0.0F, 0.0F, 0.0F),
            Vec4(0.0F, 1.0F, 0.0F, 0.0F),
            Vec4(0.0F, 0.0F, 1.0F, 0.0F),
            Vec4(0.0F, 0.0F, 0.0F, 1.0F)};

        constexpr Mat4() = default;
        constexpr explicit Mat4(float Diagonal)
            : Columns{
                Vec4(Diagonal, 0.0F, 0.0F, 0.0F),
                Vec4(0.0F, Diagonal, 0.0F, 0.0F),
                Vec4(0.0F, 0.0F, Diagonal, 0.0F),
                Vec4(0.0F, 0.0F, 0.0F, Diagonal)}
        {
        }

        [[nodiscard]] constexpr Vec4& operator[](int Column)
        {
            return Columns[Column];
        }

        [[nodiscard]] constexpr const Vec4& operator[](int Column) const
        {
            return Columns[Column];
        }

        [[nodiscard]] float* Data()
        {
            return &Columns[0].x;
        }

        [[nodiscard]] const float* Data() const
        {
            return &Columns[0].x;
        }

        [[nodiscard]] constexpr Vec4 operator*(const Vec4& Value) const
        {
            return Columns[0] * Value.x + Columns[1] * Value.y + Columns[2] * Value.z + Columns[3] * Value.w;
        }

        [[nodiscard]] constexpr Mat4 operator*(const Mat4& Right) const
        {
            Mat4 Result(0.0F);
            Result.Columns[0] = *this * Right.Columns[0];
            Result.Columns[1] = *this * Right.Columns[1];
            Result.Columns[2] = *this * Right.Columns[2];
            Result.Columns[3] = *this * Right.Columns[3];
            return Result;
        }
    };

    constexpr Mat3::Mat3(const Mat4& Value)
        : Columns{Vec3(Value.Columns[0]), Vec3(Value.Columns[1]), Vec3(Value.Columns[2])}
    {
    }
}
