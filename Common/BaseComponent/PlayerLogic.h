//-----------------------------------------------------------------------------
// File: PlayerLogic.h
// 서버/클라이언트 공용 플레이어 물리 로직
// DirectX 의존성 없음
//-----------------------------------------------------------------------------
#pragma once

#include "GameMath.h"

namespace PlayerLogic
{
    using namespace GameMath;

    // 입력 방향 상수 (매크로 DIR_*와 충돌 방지)
    constexpr int32_t PL_DIR_FORWARD = 0x01;
    constexpr int32_t PL_DIR_BACKWARD = 0x02;
    constexpr int32_t PL_DIR_LEFT = 0x04;
    constexpr int32_t PL_DIR_RIGHT = 0x08;
    constexpr int32_t PL_DIR_UP = 0x10;
    constexpr int32_t PL_DIR_DOWN = 0x20;

    inline Vec3 GetLocalMoveDirection(int32_t inputKeys)
    {
        Vec3 localDir;

        if (inputKeys & PL_DIR_FORWARD)  localDir.z += 1.f;
        if (inputKeys & PL_DIR_BACKWARD) localDir.z -= 1.f;
        if (inputKeys & PL_DIR_LEFT)     localDir.x -= 1.f;
        if (inputKeys & PL_DIR_RIGHT)    localDir.x += 1.f;

        localDir.Normalize();
        return localDir;
    }

    inline Vec3 ComputeMoveDirection(int32_t inputKeys, float yawDeg)
    {
        Vec3 localDir = GetLocalMoveDirection(inputKeys);
        if (localDir.LengthSq() < kEpsilon) return Vec3::Zero();

        Vec3 look = YawToLook(yawDeg);
        Vec3 right = YawToRight(yawDeg);

        return Vec3(
            right.x * localDir.x + look.x * localDir.z,
            0.f,
            right.z * localDir.x + look.z * localDir.z
        );
    }

    inline void ApplyInput(Vec3& velocity, int32_t inputKeys, float yawDeg, float moveSpeed, float dt)
    {
        Vec3 moveDir = ComputeMoveDirection(inputKeys, yawDeg);
        velocity += moveDir * (moveSpeed * dt);

        if (inputKeys & PL_DIR_UP)   velocity.y += moveSpeed * dt;
        if (inputKeys & PL_DIR_DOWN) velocity.y -= moveSpeed * dt;
    }

    inline void ApplyPhysics(
        Vec3& position, Vec3& velocity, const Vec3& gravity,
        float friction, float maxVelXZ, float maxVelY, float dt)
    {
        velocity += gravity * dt;

        float lenXZ = velocity.LengthXZ();
        float capXZ = maxVelXZ * dt;
        if (capXZ > 0.f && lenXZ > capXZ)
        {
            float s = capXZ / lenXZ;
            velocity.x *= s;
            velocity.z *= s;
        }

        float absY = fabsf(velocity.y);
        float capY = maxVelY * dt;
        if (capY > 0.f && absY > capY)
            velocity.y *= (capY / absY);

        position += velocity;

        float len = velocity.Length();
        float decel = friction * dt;
        if (decel > len) decel = len;

        if (len > kEpsilon)
        {
            float scale = (len - decel) / len;
            velocity *= scale;
        }
    }

    inline void UpdateMovement(
        Vec3& position, Vec3& velocity, int32_t inputKeys, float yawDeg, float moveSpeed,
        const Vec3& gravity, float friction, float maxVelXZ, float maxVelY, float dt)
    {
        ApplyInput(velocity, inputKeys, yawDeg, moveSpeed, dt);
        ApplyPhysics(position, velocity, gravity, friction, maxVelXZ, maxVelY, dt);
    }
}
