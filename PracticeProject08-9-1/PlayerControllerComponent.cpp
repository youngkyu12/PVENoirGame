//-----------------------------------------------------------------------------
// File: PlayerControllerComponent.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "PlayerControllerComponent.h"
#include "ActorTagComponent.h"

#include "Object.h"
#include "Animator.h"
#include "AnimatorComponent.h"
#include "AnimController.h"

// ���� ��Ʈ�ѷ� + ���� ����
#include "CommonPlayerControllerComponent.h"
#include "GameMath.h"

static GameMath::Vec3 ToGM(const XMFLOAT3& v)
{
    return GameMath::Vec3(v.x, v.y, v.z);
}

CPlayerControllerComponent::CPlayerControllerComponent(CGameObject* owner)
    : CComponentT<CPlayerControllerComponent>(owner)
{
    PushTuningToCommon();
}

CCommonPlayerControllerComponent* CPlayerControllerComponent::GetCommon() const
{
    CGameObject* owner = GetOwner();
    if (!owner) return nullptr;
    return owner->GetComponent<CCommonPlayerControllerComponent>();
}

bool CPlayerControllerComponent::IsAttackBlocking() const
{
    CGameObject* owner = GetOwner();
    if (!owner) return false;

    if (auto* anim = owner->GetAnimator())
    {
        if (anim->GetCurrentClipName() == "Attack" && !anim->IsCurrentClipFinished())
            return true;
    }
    return false;
}

void CPlayerControllerComponent::PushTuningToCommon()
{
    auto* common = GetCommon();
    if (!common) return;

    common->SetFriction(m_friction);
    common->SetMaxVelocityXZ(m_maxVelXZ);
    common->SetMaxVelocityY(m_maxVelY);
    common->SetGravity(ToGM(m_gravity));

    // yaw�� �ݿ�
    common->SetYawDegrees(m_yawDeg);
}

void CPlayerControllerComponent::SetFriction(float f)
{
    m_friction = f;
    PushTuningToCommon();
}

void CPlayerControllerComponent::SetGravity(const XMFLOAT3& g)
{
    m_gravity = g;
    PushTuningToCommon();
}

void CPlayerControllerComponent::SetMaxVelocityXZ(float v)
{
    m_maxVelXZ = v;
    PushTuningToCommon();
}

void CPlayerControllerComponent::SetMaxVelocityY(float v)
{
    m_maxVelY = v;
    PushTuningToCommon();
}

void CPlayerControllerComponent::SetYawDegrees(float yawDeg)
{
    // 0~360 wrap (legacy ����)
    m_yawDeg = yawDeg;
    if (m_yawDeg > 360.0f) m_yawDeg = fmodf(m_yawDeg, 360.0f);
    if (m_yawDeg < 0.0f)
    {
        m_yawDeg = fmodf(m_yawDeg, 360.0f);
        if (m_yawDeg < 0.0f) m_yawDeg += 360.0f;
    }

    if (IsAttackBlocking())
        return;

    if (auto* common = GetCommon())
        common->SetYawDegrees(m_yawDeg);
}

void CPlayerControllerComponent::SetInputDirection(DWORD dwDirection)
{
    m_inputDir = dwDirection;

    if (!IsAttackBlocking())
    {
        if (auto* common = GetCommon())
            common->SetInputDirection(static_cast<int32_t>(m_inputDir));
    }

    SyncAnimatorSpeed();
}

void CPlayerControllerComponent::SyncAnimatorSpeed()
{
    CGameObject* owner = GetOwner();
    if (!owner) return;

    if (IsAttackBlocking())
        return;

    const float speed = (m_inputDir ? 1.0f : 0.0f);

    if (auto* animComp = owner->GetComponent<CAnimatorComponent>())
    {
        animComp->SetSpeed(speed);
        return;
    }

    if (auto* ctrl = owner->GetAnimController())
    {
        ctrl->SetSpeed(speed);
        return;
    }
}

void CPlayerControllerComponent::Move(
    DWORD dwDirection,
    float fDistance,
    bool bUpdateVelocity,
    EVerticalMoveSpace /*upSpace*/)
{
    auto* owner = GetOwner();
    if (!owner) return;

    auto* tag = owner->GetComponent<CActorTagComponent>();
    if (tag && tag->kind == EActorKind::Player && tag->control != EPlayerControl::Local)
        return;

    if (IsAttackBlocking())
        return;

    if (!dwDirection) return;

    if (auto* common = GetCommon())
    {
        common->Move(
            static_cast<int32_t>(dwDirection),
            fDistance,
            bUpdateVelocity,
            CCommonPlayerControllerComponent::EVerticalMoveSpace::WorldUp
        );
    }
}

void CPlayerControllerComponent::MoveShift(const XMFLOAT3& shift, bool bUpdateVelocity)
{
    auto* owner = GetOwner();
    if (!owner) return;

    auto* tag = owner->GetComponent<CActorTagComponent>();
    if (tag && tag->kind == EActorKind::Player && tag->control != EPlayerControl::Local)
        return;

    if (IsAttackBlocking())
        return;

    if (auto* common = GetCommon())
        common->MoveShift(ToGM(shift), bUpdateVelocity);
}

void CPlayerControllerComponent::Rotate(float /*pitchDeg*/, float yawDeg, float /*rollDeg*/)
{
    auto* owner = GetOwner();
    if (!owner) return;

    auto* tag = owner->GetComponent<CActorTagComponent>();
    if (tag && tag->kind == EActorKind::Player && tag->control != EPlayerControl::Local)

        return;
    if (IsAttackBlocking())
        return;

    if (yawDeg != 0.0f)
        SetYawDegrees(m_yawDeg + yawDeg);
}

void CPlayerControllerComponent::OnUpdate(float /*dt*/)
{
    // �̵�/���� ������Ʈ�� common ��Ʈ�ѷ��� �Ѵ�.
    // Ŭ�� �긮���� �ִ� speed�� ����.
    SyncAnimatorSpeed();
}