#include "stdafx.h"
#include "AnimController.h"
#include "AnimatorComponent.h"
#include "Object.h"
#include "Animator.h"
#include "PlayerEquipmentComponent.h"
#include "GlobalEnum.h"
#include "GlobalValues.h"

namespace
{
    static std::string BuildDirectionSuffix(uint32_t bits)
    {
        const bool f = (bits & DIR_FORWARD) != 0;
        const bool b = (bits & DIR_BACKWARD) != 0;
        const bool l = (bits & DIR_LEFT) != 0;
        const bool r = (bits & DIR_RIGHT) != 0;

        if (f && !b)
        {
            if (l && !r) return "FL";
            if (r && !l) return "FR";
            return "F";
        }

        if (b && !f)
        {
            if (l && !r) return "BL";
            if (r && !l) return "BR";
            return "B";
        }

        if (l && !r) return "L";
        if (r && !l) return "R";

        return "";
    }

    static EWeaponType GetEquippedWeaponType(const CGameObject* owner)
    {
        if (!owner) return EWeaponType::None;

        auto* equip = owner->GetComponent<CPlayerEquipmentComponent>();
        if (!equip) return EWeaponType::None;

        return equip->GetEquippedWeapon();
    }
}

std::string CAnimController::ResolveIdleClip() const
{
    if (!m_usePlayerClipSet)
        return m_idleClip;

    switch (GetEquippedWeaponType(m_pOwner))
    {
    case EWeaponType::Sword: return "Idle_Sword";
    case EWeaponType::Bow:   return "Idle_Bow";
    case EWeaponType::Axe:   return "Idle_Axe";
    case EWeaponType::Gun:   return "Idle_Gun";
    default:                 return "Idle_Normal";
    }
}

std::string CAnimController::ResolveMoveClip() const
{
    if (!m_usePlayerClipSet)
        return m_moveClip;

    const std::string suffix = BuildDirectionSuffix(m_moveDirBits);
    if (suffix.empty())
        return ResolveIdleClip();

    const char* prefix = m_bRunRequested ? "Run_" : "Walk_";
    return std::string(prefix) + suffix;
}

std::string CAnimController::ResolveHitClip() const
{
    if (!m_usePlayerClipSet)
        return m_hitClip;

    switch (GetEquippedWeaponType(m_pOwner))
    {
    case EWeaponType::Sword:
    case EWeaponType::Axe:
        return "Hit_Sword";

    case EWeaponType::Bow:
    case EWeaponType::Gun:
    case EWeaponType::None:
    default:
        return "Hit_Normal";
    }
}

std::string CAnimController::ResolveAttackStartClip(EActionPhase& outPhase) const
{
    outPhase = EActionPhase::None;

    if (!m_usePlayerClipSet)
    {
        outPhase = EActionPhase::AttackGeneric;
        return m_attackClip;
    }

    switch (GetEquippedWeaponType(m_pOwner))
    {
    case EWeaponType::Sword:
        outPhase = EActionPhase::AttackGeneric;
        return "Attack_Sword";

    case EWeaponType::Axe:
        outPhase = EActionPhase::AttackGeneric;
        return "Attack_Axe";

    case EWeaponType::Bow:
        outPhase = EActionPhase::AttackBowLoad;
        return "Bow_Load";

    case EWeaponType::Gun:
        outPhase = EActionPhase::AttackGeneric;
        return "Gun_Shoot";

    case EWeaponType::None:
    default:
        return "";
    }
}

std::string CAnimController::ResolveLocomotionClip(EAnimState state) const
{
    if (state == EAnimState::Move)
        return ResolveMoveClip();

    return ResolveIdleClip();
}

void CAnimController::Update(float /*dt*/)
{
    if (!m_pOwner) return;

    CAnimator* anim = nullptr;

    if (auto* animComp = m_pOwner->GetComponent<CAnimatorComponent>())
        anim = animComp->GetAnimator();

    if (!anim)
        anim = m_pOwner->GetAnimator();

    if (!anim) return;

    // ------------------------------------------------------------
    // Attack request
    // ------------------------------------------------------------
    if (m_attackQueued)
    {
        m_attackQueued = false;

        EActionPhase nextPhase = EActionPhase::None;
        const std::string atkClip = ResolveAttackStartClip(nextPhase);

        if (!atkClip.empty() && anim->HasClip(atkClip))
        {
            constexpr float kAtkBlendTime = 0.12f;

            if (!anim->GetCurrentClipName().empty())
            {
                if (!anim->CrossFade(atkClip, kAtkBlendTime, false, 0.0f))
                    anim->Play(atkClip, false, 0.0f);
            }
            else
            {
                anim->Play(atkClip, false, 0.0f);
            }

            m_actionPhase = nextPhase;
            m_state = EAnimState::Attack;
            return;
        }
    }

    // ------------------------------------------------------------
    // Hit request
    // ------------------------------------------------------------
    if (m_hitQueued)
    {
        m_hitQueued = false;

        const std::string hitClip = ResolveHitClip();
        if (!hitClip.empty() && anim->HasClip(hitClip))
        {
            constexpr float kHitBlendTime = 0.08f;

            if (!anim->GetCurrentClipName().empty())
            {
                if (!anim->CrossFade(hitClip, kHitBlendTime, false, 0.0f))
                    anim->Play(hitClip, false, 0.0f);
            }
            else
            {
                anim->Play(hitClip, false, 0.0f);
            }

            m_actionPhase = EActionPhase::Hit;
            return;
        }
    }

    // ------------------------------------------------------------
    // Action progression
    // ------------------------------------------------------------
    if (m_actionPhase != EActionPhase::None)
    {
        if (anim->IsCurrentClipFinished())
        {
            if (m_actionPhase == EActionPhase::AttackBowLoad)
            {
                if (anim->HasClip("Bow_Release"))
                {
                    constexpr float kBowChainBlendTime = 0.05f;

                    if (!anim->CrossFade("Bow_Release", kBowChainBlendTime, false, 0.0f))
                        anim->Play("Bow_Release", false, 0.0f);

                    m_actionPhase = EActionPhase::AttackBowRelease;
                    return;
                }
            }

            const bool wantsMove =
                m_usePlayerClipSet ? (m_moveDirBits != 0)
                : (m_speed > m_moveEps);

            const EAnimState targetState = wantsMove ? EAnimState::Move : EAnimState::Idle;
            std::string targetClip = ResolveLocomotionClip(targetState);

            if (targetClip.empty() || !anim->HasClip(targetClip))
            {
                targetClip = ResolveIdleClip();
            }

            if (!targetClip.empty() && anim->HasClip(targetClip))
            {
                constexpr float kOutBlendTime = 0.12f;

                if (!anim->CrossFade(targetClip, kOutBlendTime, true, 0.0f))
                    anim->Play(targetClip, true, 0.0f);
            }

            m_actionPhase = EActionPhase::None;
            m_state = targetState;
        }
        return;
    }

#ifdef USING_NETWORK
    std::string targetClip = ResolveLocomotionClip(m_state);
    if (targetClip.empty() || !anim->HasClip(targetClip))
    {
        targetClip = ResolveIdleClip();
        if (targetClip.empty() || !anim->HasClip(targetClip))
            return;
    }

    constexpr float kBlendTime = 0.15f;

    if (anim->GetCurrentClipName().empty())
    {
        anim->Play(targetClip, true, 0.0f);
    }
    else if (m_state != animPrevState || anim->GetCurrentClipName() != targetClip)
    {
        if (!anim->CrossFade(targetClip, kBlendTime, true, 0.0f))
            anim->Play(targetClip, true, 0.0f);
    }
#else
    const bool wantsMove =
        m_usePlayerClipSet ? (m_moveDirBits != 0)
        : (m_speed > m_moveEps);

    const EAnimState targetState = wantsMove ? EAnimState::Move : EAnimState::Idle;

    std::string targetClip = ResolveLocomotionClip(targetState);
    if (targetClip.empty() || !anim->HasClip(targetClip))
    {
        targetClip = ResolveIdleClip();
        if (targetClip.empty() || !anim->HasClip(targetClip))
            return;
    }

    constexpr float kBlendTime = 0.15f;

    if (anim->GetCurrentClipName().empty())
    {
        anim->Play(targetClip, true, 0.0f);
        m_state = targetState;
    }
    else if (targetState != m_state || anim->GetCurrentClipName() != targetClip)
    {
        if (!anim->CrossFade(targetClip, kBlendTime, true, 0.0f))
            anim->Play(targetClip, true, 0.0f);

        m_state = targetState;
    }
    else
    {
        m_state = targetState;
    }
#endif

    animPrevState = m_state;
}

void CAnimController::RequestAttack()
{
    if (m_actionPhase != EActionPhase::None)
        return;

    EActionPhase phase = EActionPhase::None;
    const std::string atkClip = ResolveAttackStartClip(phase);

    if (atkClip.empty())
        return;

    m_attackQueued = true;
}

void CAnimController::RequestHit()
{
    if (m_actionPhase != EActionPhase::None)
        return;

    const std::string hitClip = ResolveHitClip();
    if (hitClip.empty())
        return;

    m_hitQueued = true;
}