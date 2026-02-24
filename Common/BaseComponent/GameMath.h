//-----------------------------------------------------------------------------
// File: GameMath.h
// 순수 C++ 수학 라이브러리 - DirectX/Windows 의존성 없음
// 서버/클라이언트 공용
//-----------------------------------------------------------------------------
#pragma once

#include <cmath>
#include <cstdint>

namespace GameMath
{
    constexpr float PI = 3.14159265358979f;
    constexpr float DEG_TO_RAD = PI / 180.f;
    constexpr float RAD_TO_DEG = 180.f / PI;
    constexpr float EPSILON = 1e-6f;

    //-------------------------------------------------------------------------
    // Vec3: 3D 벡터
    //-------------------------------------------------------------------------
    struct Vec3
    {
        float x = 0.f, y = 0.f, z = 0.f;

        Vec3() = default;
        Vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

        // 연산자
        Vec3 operator+(const Vec3& v) const { return { x + v.x, y + v.y, z + v.z }; }
        Vec3 operator-(const Vec3& v) const { return { x - v.x, y - v.y, z - v.z }; }
        Vec3 operator*(float s) const { return { x * s, y * s, z * s }; }
        Vec3 operator/(float s) const { return { x / s, y / s, z / s }; }
        Vec3 operator-() const { return { -x, -y, -z }; }

        Vec3& operator+=(const Vec3& v) { x += v.x; y += v.y; z += v.z; return *this; }
        Vec3& operator-=(const Vec3& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
        Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
        Vec3& operator/=(float s) { x /= s; y /= s; z /= s; return *this; }

        bool operator==(const Vec3& v) const { return x == v.x && y == v.y && z == v.z; }
        bool operator!=(const Vec3& v) const { return !(*this == v); }

        // 유틸리티
        float Length() const { return sqrtf(x * x + y * y + z * z); }
        float LengthSq() const { return x * x + y * y + z * z; }
        float LengthXZ() const { return sqrtf(x * x + z * z); }

        Vec3 Normalized() const
        {
            float len = Length();
            return (len > EPSILON) ? Vec3(x / len, y / len, z / len) : Vec3();
        }

        void Normalize()
        {
            float len = Length();
            if (len > EPSILON) { x /= len; y /= len; z /= len; }
        }

        static Vec3 Zero() { return { 0.f, 0.f, 0.f }; }
        static Vec3 One() { return { 1.f, 1.f, 1.f }; }
        static Vec3 Up() { return { 0.f, 1.f, 0.f }; }
        static Vec3 Forward() { return { 0.f, 0.f, 1.f }; }
        static Vec3 Right() { return { 1.f, 0.f, 0.f }; }

        static float Dot(const Vec3& a, const Vec3& b)
        {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        }

        static Vec3 Cross(const Vec3& a, const Vec3& b)
        {
            return {
                a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x
            };
        }

        static Vec3 Lerp(const Vec3& a, const Vec3& b, float t)
        {
            return a + (b - a) * t;
        }
    };

    inline Vec3 operator*(float s, const Vec3& v) { return v * s; }

    //-------------------------------------------------------------------------
    // Vec4: 4D 벡터 / 쿼터니언
    //-------------------------------------------------------------------------
    struct Vec4
    {
        float x = 0.f, y = 0.f, z = 0.f, w = 1.f;

        Vec4() = default;
        Vec4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}

        static Vec4 Identity() { return { 0.f, 0.f, 0.f, 1.f }; }
    };

    //-------------------------------------------------------------------------
    // Mat4x4: 4x4 행렬 (row-major)
    //-------------------------------------------------------------------------
    struct Mat4x4
    {
        float m[4][4] = {
            {1,0,0,0},
            {0,1,0,0},
            {0,0,1,0},
            {0,0,0,1}
        };

        static Mat4x4 Identity()
        {
            Mat4x4 result;
            return result;
        }

        static Mat4x4 Translation(float x, float y, float z)
        {
            Mat4x4 result;
            result.m[3][0] = x;
            result.m[3][1] = y;
            result.m[3][2] = z;
            return result;
        }

        static Mat4x4 Translation(const Vec3& v)
        {
            return Translation(v.x, v.y, v.z);
        }

        static Mat4x4 RotationY(float radians)
        {
            Mat4x4 result;
            float c = cosf(radians);
            float s = sinf(radians);
            result.m[0][0] = c;
            result.m[0][2] = -s;
            result.m[2][0] = s;
            result.m[2][2] = c;
            return result;
        }

        static Mat4x4 Scale(float x, float y, float z)
        {
            Mat4x4 result;
            result.m[0][0] = x;
            result.m[1][1] = y;
            result.m[2][2] = z;
            return result;
        }

        static Mat4x4 Scale(const Vec3& v)
        {
            return Scale(v.x, v.y, v.z);
        }

        Mat4x4 operator*(const Mat4x4& other) const
        {
            Mat4x4 result;
            for (int i = 0; i < 4; ++i)
            {
                for (int j = 0; j < 4; ++j)
                {
                    result.m[i][j] = 0.f;
                    for (int k = 0; k < 4; ++k)
                    {
                        result.m[i][j] += m[i][k] * other.m[k][j];
                    }
                }
            }
            return result;
        }

        Vec3 TransformPoint(const Vec3& v) const
        {
            return {
                v.x * m[0][0] + v.y * m[1][0] + v.z * m[2][0] + m[3][0],
                v.x * m[0][1] + v.y * m[1][1] + v.z * m[2][1] + m[3][1],
                v.x * m[0][2] + v.y * m[1][2] + v.z * m[2][2] + m[3][2]
            };
        }

        Vec3 TransformVector(const Vec3& v) const
        {
            return {
                v.x * m[0][0] + v.y * m[1][0] + v.z * m[2][0],
                v.x * m[0][1] + v.y * m[1][1] + v.z * m[2][1],
                v.x * m[0][2] + v.y * m[1][2] + v.z * m[2][2]
            };
        }
    };

    //-------------------------------------------------------------------------
    // 유틸리티 함수
    //-------------------------------------------------------------------------
    
    // Yaw(도) → Look 벡터
    inline Vec3 YawToLook(float yawDeg)
    {
        float rad = yawDeg * DEG_TO_RAD;
        return { sinf(rad), 0.f, cosf(rad) };
    }

    // Yaw(도) → Right 벡터
    inline Vec3 YawToRight(float yawDeg)
    {
        float rad = yawDeg * DEG_TO_RAD;
        return { cosf(rad), 0.f, -sinf(rad) };
    }

    // Yaw 정규화 (0 ~ 360)
    inline float NormalizeYaw(float yaw)
    {
        yaw = fmodf(yaw, 360.f);
        if (yaw < 0.f) yaw += 360.f;
        return yaw;
    }

    // Clamp
    inline float Clamp(float value, float minVal, float maxVal)
    {
        if (value < minVal) return minVal;
        if (value > maxVal) return maxVal;
        return value;
    }

    // Lerp
    inline float Lerp(float a, float b, float t)
    {
        return a + (b - a) * t;
    }

} // namespace GameMath
