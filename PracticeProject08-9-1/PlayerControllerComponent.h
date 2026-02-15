//-----------------------------------------------------------------------------
// File: PlayerControllerComponent.h
//-----------------------------------------------------------------------------
#pragma once

#include "Component.h"

class CGameObject;

class CPlayerControllerComponent final : public CComponentT<CPlayerControllerComponent>
{
public:
    enum class EVerticalMoveSpace : uint8_t
    {
        WorldUp,
        LocalUp
    };

public:
    explicit CPlayerControllerComponent(CGameObject* owner);

    // ----------------------------
    // Input -> (Animator speed)
    // ----------------------------
    void SetInputDirection(DWORD dwDirection);
    DWORD GetInputDirection() const { return m_inputDir; }

    // ----------------------------
    // API (legacy CPlayer mirror)
    // ----------------------------
    void Move(DWORD dwDirection, float fDistance, bool bUpdateVelocity = false,
        EVerticalMoveSpace upSpace = EVerticalMoveSpace::WorldUp);

    void MoveShift(const XMFLOAT3& shift, bool bUpdateVelocity = false);

    void Rotate(float pitchDeg, float yawDeg, float rollDeg); // legacy: yaw-only

    // ----------------------------
    // Tuning / State accessors
    // ----------------------------
    void SetFriction(float f) { m_friction = f; }
    void SetGravity(const XMFLOAT3& g) { m_gravity = g; }
    void SetMaxVelocityXZ(float v) { m_maxVelXZ = v; }
    void SetMaxVelocityY(float v) { m_maxVelY = v; }
    void SetVelocity(const XMFLOAT3& v) { m_velocity = v; }
    void SetYawDegrees(float yawDeg);

    const XMFLOAT3& GetVelocity() const { return m_velocity; }
    float GetYawDegrees() const { return m_yawDeg; }

    // ----------------------------
    // Lifecycle
    // ----------------------------
    void OnUpdate(float dt) override;

private:
    void ApplyYawToOwnerTransform();
    void SyncAnimatorSpeed();

private:
    XMFLOAT3 m_velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
    XMFLOAT3 m_gravity = XMFLOAT3(0.0f, 0.0f, 0.0f);

    float    m_yawDeg = 0.0f; // degrees
    float    m_maxVelXZ = 0.0f;
    float    m_maxVelY = 0.0f;
    float    m_friction = 0.0f;

    DWORD    m_inputDir = 0;    // 마지막 입력 방향(애니 speed 제어용)
};
