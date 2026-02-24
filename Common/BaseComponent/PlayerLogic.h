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

    //-------------------------------------------------------------------------
    // 입력 방향 상수
    //-------------------------------------------------------------------------
    constexpr int32_t DIR_FORWARD  = 0x01;
    constexpr int32_t DIR_BACKWARD = 0x02;
    constexpr int32_t DIR_LEFT     = 0x04;
    constexpr int32_t DIR_RIGHT    = 0x08;
    constexpr int32_t DIR_UP       = 0x10;
    constexpr int32_t DIR_DOWN     = 0x20;

    //-------------------------------------------------------------------------
    // 입력 → 로컬 이동 방향 계산 (정규화됨)
    //-------------------------------------------------------------------------
    inline Vec3 GetLocalMoveDirection(int32_t inputKeys)
    {
        Vec3 localDir;
        
        if (inputKeys & DIR_FORWARD)  localDir.z += 1.f;
        if (inputKeys & DIR_BACKWARD) localDir.z -= 1.f;
        if (inputKeys & DIR_LEFT)     localDir.x -= 1.f;
        if (inputKeys & DIR_RIGHT)    localDir.x += 1.f;

        localDir.Normalize();
        return localDir;
    }

    //-------------------------------------------------------------------------
    // 입력 → 월드 이동 방향 계산
    //-------------------------------------------------------------------------
    inline Vec3 ComputeMoveDirection(int32_t inputKeys, float yawDeg)
    {
        Vec3 localDir = GetLocalMoveDirection(inputKeys);
        
        if (localDir.LengthSq() < EPSILON)
            return Vec3::Zero();

        // 로컬 → 월드 변환
        Vec3 look = YawToLook(yawDeg);
        Vec3 right = YawToRight(yawDeg);

        return Vec3(
            right.x * localDir.x + look.x * localDir.z,
            0.f,
            right.z * localDir.x + look.z * localDir.z
        );
    }

    //-------------------------------------------------------------------------
    // 속도에 이동 입력 적용
    //-------------------------------------------------------------------------
    inline void ApplyInput(
        Vec3& velocity,
        int32_t inputKeys,
        float yawDeg,
        float moveSpeed,
        float dt)
    {
        Vec3 moveDir = ComputeMoveDirection(inputKeys, yawDeg);
        velocity += moveDir * (moveSpeed * dt);

        // 수직 이동 (점프/하강)
        if (inputKeys & DIR_UP)
            velocity.y += moveSpeed * dt;
        if (inputKeys & DIR_DOWN)
            velocity.y -= moveSpeed * dt;
    }

    //-------------------------------------------------------------------------
    // 물리 적용: 중력, 속도 제한, 마찰
    //-------------------------------------------------------------------------
    inline void ApplyPhysics(
        Vec3& position,
        Vec3& velocity,
        const Vec3& gravity,
        float friction,
        float maxVelXZ,
        float maxVelY,
        float dt)
    {
        // 1. 중력
        velocity += gravity * dt;

        // 2. XZ 속도 제한
        float lenXZ = velocity.LengthXZ();
        float capXZ = maxVelXZ * dt;
        if (capXZ > 0.f && lenXZ > capXZ)
        {
            float s = capXZ / lenXZ;
            velocity.x *= s;
            velocity.z *= s;
        }

        // 3. Y 속도 제한
        float absY = fabsf(velocity.y);
        float capY = maxVelY * dt;
        if (capY > 0.f && absY > capY)
        {
            velocity.y *= (capY / absY);
        }

        // 4. 위치 적용
        position += velocity;

        // 5. 마찰
        float len = velocity.Length();
        float decel = friction * dt;
        if (decel > len) decel = len;

        if (len > EPSILON)
        {
            float scale = (len - decel) / len;
            velocity *= scale;
        }
    }

    //-------------------------------------------------------------------------
    // 이동 + 물리를 한 번에 처리하는 편의 함수
    //-------------------------------------------------------------------------
    inline void UpdateMovement(
        Vec3& position,
        Vec3& velocity,
        int32_t inputKeys,
        float yawDeg,
        float moveSpeed,
        const Vec3& gravity,
        float friction,
        float maxVelXZ,
        float maxVelY,
        float dt)
    {
        ApplyInput(velocity, inputKeys, yawDeg, moveSpeed, dt);
        ApplyPhysics(position, velocity, gravity, friction, maxVelXZ, maxVelY, dt);
    }

} // namespace PlayerLogic
