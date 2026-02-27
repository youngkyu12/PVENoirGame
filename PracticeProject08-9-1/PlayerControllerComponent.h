//-----------------------------------------------------------------------------
// File: PlayerControllerComponent.h
// Ŭ���̾�Ʈ ���� "�긮��" ������Ʈ
// - �̵�/����/���� ����: CCommonPlayerControllerComponent�� ����
// - Ŭ�� ����: Animator/AnimController speed ����, Attack �� �Է�/�̵�/ȸ�� ����
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

    // Movement API (legacy signature ����) - ���� �̵��� common ��Ʈ�ѷ��� ����
    void Move(DWORD dwDirection, float fDistance, bool bUpdateVelocity = false,
        EVerticalMoveSpace upSpace = EVerticalMoveSpace::WorldUp);

    void MoveShift(const XMFLOAT3& shift, bool bUpdateVelocity = false);

    void Rotate(float pitchDeg, float yawDeg, float rollDeg); // legacy: yaw-only
    void SetYawDegrees(float yawDeg);
    float GetYawDegrees() const { return m_yawDeg; }

    // Tuning (common���� ����)
    void SetFriction(float f);
    void SetGravity(const XMFLOAT3& g);
    void SetMaxVelocityXZ(float v);
    void SetMaxVelocityY(float v);

    // legacy ������
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
