//-----------------------------------------------------------------------------
// File: PlayerControllerComponent.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "PlayerControllerComponent.h"

#include "Object.h"
#include "AnimatorComponent.h"
#include "AnimController.h"

// legacy DIR_* 값과 동일한 상수(매크로에 의존하지 않기 위해 로컬 상수로 둠)
static constexpr DWORD kDirForward = 0x01;
static constexpr DWORD kDirBackward = 0x02;
static constexpr DWORD kDirLeft = 0x04;
static constexpr DWORD kDirRight = 0x08;
static constexpr DWORD kDirUp = 0x10;
static constexpr DWORD kDirDown = 0x20;

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
    m_inputDir = dwDirection;
    SyncAnimatorSpeed();
}

void CPlayerControllerComponent::SyncAnimatorSpeed()
{
    CGameObject* owner = GetOwner();
    if (!owner) return;

    const float speed = (m_inputDir ? 1.0f : 0.0f);

    if (auto* animComp = owner->GetComponent<CAnimatorComponent>())
        animComp->SetSpeed(speed);
    else if (auto* ctrl = owner->GetAnimController())
        ctrl->SetSpeed(speed);
}

void CPlayerControllerComponent::Move(
    DWORD dwDirection,
    float fDistance,
    bool bUpdateVelocity,
    EVerticalMoveSpace upSpace)
{
    CGameObject* owner = GetOwner();
    if (!owner) return;

    if (!dwDirection) return;

    const XMFLOAT3 look = owner->GetLook();
    const XMFLOAT3 right = owner->GetRight();

    const XMFLOAT3 up = (upSpace == EVerticalMoveSpace::LocalUp)
        ? owner->GetUp()
        : XMFLOAT3(0.0f, 1.0f, 0.0f);

    XMFLOAT3 shift = XMFLOAT3(0.0f, 0.0f, 0.0f);

    if (dwDirection & kDirForward)
        shift = Vector3::Add(shift, look, fDistance);

    if (dwDirection & kDirBackward)
        shift = Vector3::Add(shift, look, -fDistance);

    if (dwDirection & kDirRight)
        shift = Vector3::Add(shift, right, fDistance);

    if (dwDirection & kDirLeft)
        shift = Vector3::Add(shift, right, -fDistance);

    if (dwDirection & kDirUp)
        shift = Vector3::Add(shift, up, fDistance);

    if (dwDirection & kDirDown)
        shift = Vector3::Add(shift, up, -fDistance);

    MoveShift(shift, bUpdateVelocity);
}

void CPlayerControllerComponent::MoveShift(const XMFLOAT3& shift, bool bUpdateVelocity)
{
    CGameObject* owner = GetOwner();
    if (!owner) return;

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

void CPlayerControllerComponent::Rotate(float /*pitchDeg*/, float yawDeg, float /*rollDeg*/)
{
    if (yawDeg != 0.0f)
        SetYawDegrees(m_yawDeg + yawDeg);
}

void CPlayerControllerComponent::OnUpdate(float dt)
{
    // legacy CPlayer::Update 로직을 컴포넌트로 이동
    // (velocity는 “프레임 변위”처럼 쓰는 기존 방식을 그대로 유지)

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

    if (auto* ctrl = owner->GetAnimController())
    {
        const float speed = (m_inputDir != 0) ? 1.0f : 0.0f; // 또는 실제 이동속도
        ctrl->SetSpeed(speed);
    }
}


void CPlayerControllerComponent::Update(float dt)
{
}