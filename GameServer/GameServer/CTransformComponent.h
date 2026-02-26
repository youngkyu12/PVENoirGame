//-----------------------------------------------------------------------------
// File: CTransformComponent.h
// Transform 컴포넌트 - DirectX 의존성 없음
//-----------------------------------------------------------------------------
#pragma once

#include "BaseComponent.h"
#include "GameMath.h"

class CTransformComponent final : public CComponentT<CTransformComponent>
{
public:
    explicit CTransformComponent(CServerObject* owner)
        : CComponentT(owner)
    {
    }

    // 위치
    GameMath::Vec3 position = GameMath::Vec3::Zero();
    
    // 회전 (Euler angles in degrees)
    float yaw = 0.f;    // Y축 회전
    float pitch = 0.f;  // X축 회전
    float roll = 0.f;   // Z축 회전

    // 스케일
    GameMath::Vec3 scale = GameMath::Vec3::One();

    // --------------------
    // Position API
    // --------------------
    void SetPosition(const GameMath::Vec3& p) { position = p; }
    void SetPosition(float x, float y, float z) { position = { x, y, z }; }
    
    GameMath::Vec3 GetPosition() const { return position; }

    void Translate(const GameMath::Vec3& delta)
    {
        position += delta;
    }

    void Translate(float dx, float dy, float dz)
    {
        position.x += dx;
        position.y += dy;
        position.z += dz;
    }

    // --------------------
    // Rotation API
    // --------------------
    void SetYawDegrees(float deg)
    {
        yaw = GameMath::NormalizeYaw(deg);
    }

    float GetYawDegrees() const { return yaw; }

    void SetRotation(float pitchDeg, float yawDeg, float rollDeg)
    {
        pitch = pitchDeg;
        yaw = GameMath::NormalizeYaw(yawDeg);
        roll = rollDeg;
    }

    void RotateYaw(float deltaDeg)
    {
        yaw = GameMath::NormalizeYaw(yaw + deltaDeg);
    }

    // --------------------
    // Direction Vectors
    // --------------------
    GameMath::Vec3 GetLook() const
    {
        return GameMath::YawToLook(yaw);
    }

    GameMath::Vec3 GetRight() const
    {
        return GameMath::YawToRight(yaw);
    }

    GameMath::Vec3 GetUp() const
    {
        return GameMath::Vec3::Up();
    }

    // --------------------
    // Scale API
    // --------------------
    void SetScale(const GameMath::Vec3& s) { scale = s; }
    void SetScale(float x, float y, float z) { scale = { x, y, z }; }
    void SetUniformScale(float s) { scale = { s, s, s }; }
    
    GameMath::Vec3 GetScale() const { return scale; }

    // --------------------
    // World Matrix
    // --------------------
    GameMath::Mat4x4 GetWorldMatrix() const
    {
        GameMath::Mat4x4 S = GameMath::Mat4x4::Scale(scale);
        GameMath::Mat4x4 R = GameMath::Mat4x4::RotationY(yaw * GameMath::DEG_TO_RAD);
        GameMath::Mat4x4 T = GameMath::Mat4x4::Translation(position);
        return S * R * T;
    }
};
