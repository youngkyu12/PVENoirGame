//-----------------------------------------------------------------------------
// File: PlayerControllerComponent.h
// 클라이언트 전용 "브리지" 컴포넌트
// - 이동/물리/권위 상태: CCommonPlayerControllerComponent로 위임
// - 클라 전용: Animator/AnimController speed 갱신, Attack 중 입력/이동/회전 차단
//-----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include "Component.h"

class CGameObject;
class CCommonPlayerControllerComponent;

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

    // Input
    void SetInputDirection(DWORD dwDirection);
    DWORD GetInputDirection() const { return m_inputDir; }

    // Movement API (legacy signature 유지) - 실제 이동은 common 컨트롤러로 위임
    void Move(DWORD dwDirection, float fDistance, bool bUpdateVelocity = false,
        EVerticalMoveSpace upSpace = EVerticalMoveSpace::WorldUp);

    void MoveShift(const XMFLOAT3& shift, bool bUpdateVelocity = false);

    void Rotate(float pitchDeg, float yawDeg, float rollDeg); // legacy: yaw-only
    void SetYawDegrees(float yawDeg);
    float GetYawDegrees() const { return m_yawDeg; }

    // Tuning (common으로 전달)
    void SetFriction(float f);
    void SetGravity(const XMFLOAT3& g);
    void SetMaxVelocityXZ(float v);
    void SetMaxVelocityY(float v);

    // legacy 유지용
    void SetVelocity(const XMFLOAT3& v) { m_velocity = v; }
    const XMFLOAT3& GetVelocity() const { return m_velocity; }

    // Lifecycle
    void OnUpdate(float dt) override;

private:
    bool IsAttackBlocking() const;
    void SyncAnimatorSpeed();

    CCommonPlayerControllerComponent* GetCommon() const;
    void PushTuningToCommon();

private:
    DWORD    m_inputDir = 0;
    float    m_yawDeg = 0.0f;

    XMFLOAT3 m_velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
    XMFLOAT3 m_gravity = XMFLOAT3(0.0f, 0.0f, 0.0f);

    float    m_maxVelXZ = 0.0f;
    float    m_maxVelY = 0.0f;
    float    m_friction = 0.0f;
};