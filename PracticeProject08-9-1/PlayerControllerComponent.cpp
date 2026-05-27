//-----------------------------------------------------------------------------
// File: PlayerControllerComponent.cpp
//-----------------------------------------------------------------------------
#include "..\..\PracticeProject08-9-1\stdafx.h"
#include "stdafx.h"
#include "PlayerControllerComponent.h"

#include "Object.h"
#include "Animator.h"
#include "AnimatorComponent.h"
#include "AnimController.h"

static constexpr DWORD kDirForward = 0x01;
static constexpr DWORD kDirBackward = 0x02;
static constexpr DWORD kDirLeft = 0x04;
static constexpr DWORD kDirRight = 0x08;
static constexpr DWORD kDirUp = 0x10;
static constexpr DWORD kDirDown = 0x20;

static float WrapAngle180(float deg)
{
	while ( deg > 180.0f ) deg -= 360.0f;
	while ( deg < -180.0f ) deg += 360.0f;
	return deg;
}

static bool IsOwnerActionLocked(CGameObject* owner)
{
    if (!owner) return false;

    if (auto* animComp = owner->GetComponent<CAnimatorComponent>())
    {
        if (auto* ctrl = animComp->GetController())
            return ctrl->IsActionLocked();
    }

    if (auto* ctrl = owner->GetAnimController())
        return ctrl->IsActionLocked();

    return false;
}

static bool CanOwnerUseRunLocomotion(CGameObject* owner)
{
	if ( !owner )
		return true;

	if ( auto* animComp = owner->GetComponent<CAnimatorComponent>() )
	{
		if ( auto* ctrl = animComp->GetController() )
			return ctrl->CanUseRunLocomotion();
	}

	if ( auto* ctrl = owner->GetAnimController() )
		return ctrl->CanUseRunLocomotion();

	return true;
}

CPlayerControllerComponent::CPlayerControllerComponent(CGameObject* owner)
    : CComponentT<CPlayerControllerComponent>(owner)
{
}

void CPlayerControllerComponent::SetYawDegrees(float yawDeg)
{
    m_yawDeg = yawDeg;

    // 0~360 wrap
    if (m_yawDeg > 360.0f) m_yawDeg = fmodf(m_yawDeg, 360.0f);
    if (m_yawDeg < 0.0f)
    {
        m_yawDeg = fmodf(m_yawDeg, 360.0f);
        if (m_yawDeg < 0.0f) m_yawDeg += 360.0f;
    }

    ApplyYawToOwnerTransform();
}

void CPlayerControllerComponent::ApplyYawToOwnerTransform()
{
    CGameObject* owner = GetOwner();
    if (!owner) return;

    if (auto* tr = owner->GetComponent<CTransformComponent>())
        tr->SetYawDegrees(m_yawDeg);
}

void CPlayerControllerComponent::SetInputDirection(DWORD dwDirection)
{
	if ( !m_inputEnabled )
	{
		m_inputDir = 0;
		SyncAnimatorLocomotion();
		return;
	}

	m_inputDir = dwDirection;
	SyncAnimatorLocomotion();
}


void CPlayerControllerComponent::SyncAnimatorLocomotion()
{
    CGameObject* owner = GetOwner();
    if (!owner) return;

	const DWORD horizontalDirBits =
		m_inputDir & ( kDirForward | kDirBackward | kDirLeft | kDirRight );

	const float speed = ( horizontalDirBits ? 1.0f : 0.0f );
	const bool runRequested = IsEffectiveRunRequested();

    if (auto* animComp = owner->GetComponent<CAnimatorComponent>())
    {
        auto* ctrl = animComp->EnsureController();
        if (ctrl)
        {
			ctrl->SetSpeed(speed);
			ctrl->SetMoveDirection(static_cast< uint32_t >( horizontalDirBits ));
			ctrl->SetRunRequested(runRequested);
        }
    }
    else if (auto* ctrl = owner->GetAnimController())
    {
		ctrl->SetSpeed(speed);
		ctrl->SetMoveDirection(static_cast< uint32_t >( horizontalDirBits ));
		ctrl->SetRunRequested(runRequested);
    }
}

void CPlayerControllerComponent::Move(
	DWORD dwDirection,
	float fDistance,
	bool bUpdateVelocity,
	EVerticalMoveSpace upSpace)
{
	MoveByYaw(dwDirection, fDistance, m_yawDeg, bUpdateVelocity, upSpace);
}

void CPlayerControllerComponent::MoveByYaw(
	DWORD dwDirection,
	float fDistance,
	float yawDeg,
	bool bUpdateVelocity,
	EVerticalMoveSpace upSpace)
{
	CGameObject* owner = GetOwner();
	if ( !owner ) return;

	if ( !m_inputEnabled ) return;
	if ( IsOwnerActionLocked(owner) ) return;
	if ( !dwDirection ) return;

	const XMMATRIX yawM = XMMatrixRotationY(XMConvertToRadians(yawDeg));

	XMVECTOR lookV = XMVector3Normalize(
		XMVector3TransformNormal(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), yawM)
	);

	XMVECTOR rightV = XMVector3Normalize(
		XMVector3TransformNormal(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), yawM)
	);

	XMFLOAT3 ownerUp = owner->GetUp();
	XMVECTOR upV =
		( upSpace == EVerticalMoveSpace::LocalUp )
		? XMVector3Normalize(XMLoadFloat3(&ownerUp))
		: XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMVECTOR horizontalMoveV = XMVectorZero();
	XMVECTOR verticalMoveV = XMVectorZero();

	if ( dwDirection & kDirForward )  horizontalMoveV += lookV;
	if ( dwDirection & kDirBackward ) horizontalMoveV -= lookV;
	if ( dwDirection & kDirRight )    horizontalMoveV += rightV;
	if ( dwDirection & kDirLeft )     horizontalMoveV -= rightV;

	if ( dwDirection & kDirUp )       verticalMoveV += upV;
	if ( dwDirection & kDirDown )     verticalMoveV -= upV;

	if ( XMVectorGetX(XMVector3LengthSq(horizontalMoveV)) > 1e-8f )
	{
		horizontalMoveV = XMVector3Normalize(horizontalMoveV) * fDistance;
	}
	else
	{
		horizontalMoveV = XMVectorZero();
	}

	if ( XMVectorGetX(XMVector3LengthSq(verticalMoveV)) > 1e-8f )
	{
		verticalMoveV = XMVector3Normalize(verticalMoveV) * fDistance;
	}
	else
	{
		verticalMoveV = XMVectorZero();
	}

	XMVECTOR shiftV = horizontalMoveV + verticalMoveV;

	XMFLOAT3 shift{};
	XMStoreFloat3(&shift, shiftV);

	MoveShift(shift, bUpdateVelocity);
}

void CPlayerControllerComponent::MoveShift(const XMFLOAT3& shift, bool bUpdateVelocity)
{
	CGameObject* owner = GetOwner();
	if ( !owner ) return;
	if ( !m_inputEnabled ) return;

    if (bUpdateVelocity)
    {
        m_velocity = Vector3::Add(m_velocity, shift);
    }
    else
    {
        if (auto* tr = owner->GetComponent<CTransformComponent>())
            tr->Translate(shift);
    }
}

void CPlayerControllerComponent::RotateTowardYawDegrees(float targetYawDeg, float turnSpeed, float dt)
{
	if ( !m_inputEnabled )
		return;

	float delta = WrapAngle180(targetYawDeg - m_yawDeg);

	float alpha = turnSpeed * dt;
	if ( alpha < 0.0f ) alpha = 0.0f;
	if ( alpha > 1.0f ) alpha = 1.0f;

	SetYawDegrees(m_yawDeg + delta * alpha);
}

void CPlayerControllerComponent::Rotate(float /*pitchDeg*/, float yawDeg, float /*rollDeg*/)
{
	CGameObject* owner = GetOwner();
	if ( !m_inputEnabled ) return;
	if ( IsOwnerActionLocked(owner) ) return;
    if (yawDeg != 0.0f)
        SetYawDegrees(m_yawDeg + yawDeg);
}

bool CPlayerControllerComponent::IsActionLockedByAnimation() const
{
	return IsOwnerActionLocked(GetOwner());
}

bool CPlayerControllerComponent::ShouldFaceCameraWhileActionActive() const
{
	CGameObject* owner = GetOwner();
	if ( !owner ) return false;

	if ( auto* animComp = owner->GetComponent<CAnimatorComponent>() )
	{
		if ( auto* ctrl = animComp->GetController() )
			return ctrl->IsActionActive() && !ctrl->IsActionLocked();
	}

	if ( auto* ctrl = owner->GetAnimController() )
		return ctrl->IsActionActive() && !ctrl->IsActionLocked();

	return false;
}

void CPlayerControllerComponent::OnUpdate(float dt)
{
	if ( !m_inputEnabled )
	{
		m_velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
		SyncAnimatorLocomotion();
		return;
	}

	// gravity
	m_velocity = Vector3::Add(
        m_velocity,
        Vector3::ScalarProduct(m_gravity, dt, false)
    );

    // clamp XZ
    float lenXZ = sqrtf(m_velocity.x * m_velocity.x + m_velocity.z * m_velocity.z);
    float maxXZ = m_maxVelXZ * dt;
    if (maxXZ > 0.0f && lenXZ > maxXZ)
    {
        float s = (maxXZ / lenXZ);
        m_velocity.x *= s;
        m_velocity.z *= s;
    }

    // clamp Y
    float lenY = fabsf(m_velocity.y);
    float maxY = m_maxVelY * dt;
    if (maxY > 0.0f && lenY > maxY)
        m_velocity.y *= (maxY / lenY);

    // apply displacement
    MoveShift(m_velocity, false);

    // friction / deceleration
    float len = Vector3::Length(m_velocity);
    float decel = (m_friction * dt);

    if (decel > len) decel = len;

    m_velocity = Vector3::Add(
        m_velocity,
        Vector3::ScalarProduct(m_velocity, -decel, true) // normalize
    );

    CGameObject* owner = GetOwner();
    if (!owner) return;
    SyncAnimatorLocomotion();
}

void CPlayerControllerComponent::SetInputEnabled(bool enabled)
{
	m_inputEnabled = enabled;

	if ( !m_inputEnabled )
	{
		m_inputDir = 0;
		m_isRunRequested = false;
		m_velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
		SyncAnimatorLocomotion();
	}
}

void CPlayerControllerComponent::SetRunRequested(bool run)
{
	if ( !m_inputEnabled )
		run = false;

	m_isRunRequested = run;
	SyncAnimatorLocomotion();
}

bool CPlayerControllerComponent::IsEffectiveRunRequested() const
{
	if ( !m_inputEnabled )
		return false;

	const DWORD horizontalDirBits =
		m_inputDir & ( kDirForward | kDirBackward | kDirLeft | kDirRight );

	if ( horizontalDirBits == 0 )
		return false;

	if ( !m_isRunRequested )
		return false;

	return CanOwnerUseRunLocomotion(GetOwner());
}

bool CPlayerControllerComponent::IsRunLocomotionActive() const
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return false;

	if ( !m_inputEnabled )
		return false;

	const DWORD horizontalDirBits =
		m_inputDir & ( kDirForward | kDirBackward | kDirLeft | kDirRight );

	if ( horizontalDirBits == 0 )
		return false;

	if ( !m_isRunRequested )
		return false;

	if ( auto* animComp = owner->GetComponent<CAnimatorComponent>() )
	{
		if ( auto* ctrl = animComp->GetController() )
			return ctrl->IsRunLocomotionActive();
	}

	if ( auto* ctrl = owner->GetAnimController() )
		return ctrl->IsRunLocomotionActive();

	return false;
}