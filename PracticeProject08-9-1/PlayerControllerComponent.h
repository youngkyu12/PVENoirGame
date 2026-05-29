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

    void Update(float dt) {}
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

	void MoveByYaw(
	DWORD dwDirection,
	float fDistance,
	float yawDeg,
	bool bUpdateVelocity = false,
	EVerticalMoveSpace upSpace = EVerticalMoveSpace::WorldUp);

	void RotateTowardYawDegrees(float targetYawDeg, float turnSpeed, float dt);

	bool IsActionLockedByAnimation() const;
	bool ShouldFaceCameraWhileActionActive() const;

    // ----------------------------
    // Lifecycle
    // ----------------------------
    void OnUpdate(float dt) override;

public:
	void SetRunRequested(bool run);
	bool IsRunRequested() const { return m_isRunRequested; }
	bool IsEffectiveRunRequested() const;
	bool IsRunLocomotionActive() const;

	void SetWalkMoveSpeed(float speed);
	void SetRunMoveSpeed(float speed);
	void SetMoveSpeeds(float walkSpeed, float runSpeed);
	float GetWalkMoveSpeed() const { return m_walkMoveSpeed; }
	float GetRunMoveSpeed() const { return m_runMoveSpeed; }
	float GetCurrentMoveSpeed() const;

	void SetInputEnabled(bool enabled);
	bool IsInputEnabled() const { return m_inputEnabled; }

private:

    void ApplyYawToOwnerTransform();
    void SyncAnimatorLocomotion();

private:
	XMFLOAT3 m_velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 m_gravity = XMFLOAT3(0.0f, 0.0f, 0.0f);

	float    m_yawDeg = 0.0f;
	float    m_maxVelXZ = 0.0f;
	float    m_maxVelY = 0.0f;
	float    m_friction = 0.0f;

	float    m_walkMoveSpeed = 5.0f;
	float    m_runMoveSpeed = 10.0f;

	DWORD    m_inputDir = 0;
	bool     m_isRunRequested = false;
	bool     m_inputEnabled = true;
};
