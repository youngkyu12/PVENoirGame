//------------------------------------------------------- ----------------------
// File: AnimatorController.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "AnimController.h"
#include "AnimatorComponent.h"
#include "Object.h"
#include "Animator.h"
#include "PlayerEquipmentComponent.h"
#include "PlayerControllerComponent.h"
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
    static bool ShouldUseUpperBodyAttackOverlay(EWeaponType weapon)
    {
        return (weapon == EWeaponType::Bow) || (weapon == EWeaponType::Gun);
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

    const EWeaponType weapon = GetEquippedWeaponType(m_pOwner);

    auto IsOverlayActionPhase = [&](EActionPhase phase) -> bool
        {
            switch (phase)
            {
            case EActionPhase::AttackBowLoad:
            case EActionPhase::AttackBowRelease:
                return true;

            case EActionPhase::AttackGeneric:
                return ShouldUseUpperBodyAttackOverlay(weapon);

            default:
                return false;
            }
        };

    auto WantsMoveNow = [&]() -> bool
        {
            const bool hasLocalPlayerController =
                (m_pOwner->GetComponent<CPlayerControllerComponent>() != nullptr);

            if (m_usePlayerClipSet)
            {
                if (m_moveDirBits != 0)
                    return true;

                if (!hasLocalPlayerController && m_state == EAnimState::Move)
                    return true;

                return false;
            }

            if (m_speed > m_moveEps)
                return true;

            if (!hasLocalPlayerController && m_state == EAnimState::Move)
                return true;

            return false;
        };

    auto ResolveSafeLocomotionClip = [&]() -> std::string
        {
            const EAnimState targetState = WantsMoveNow() ? EAnimState::Move : EAnimState::Idle;

            std::string clip = ResolveLocomotionClip(targetState);
            if (clip.empty() || !anim->HasClip(clip))
                clip = ResolveIdleClip();

            return clip;
        };

    auto StartLocomotionClip = [&](const std::string& clipName)
        {
            if (clipName.empty() || !anim->HasClip(clipName))
                return;

            constexpr float kBlendTime = 0.15f;

            if (anim->GetCurrentClipName().empty())
            {
                anim->Play(clipName, true, 0.0f);
                return;
            }

            if (anim->GetCurrentClipName() != clipName)
            {
                if (!anim->CrossFade(clipName, kBlendTime, true, 0.0f))
                    anim->Play(clipName, true, 0.0f);
            }
        };

    auto StartUpperBodyAttack = [&](const std::string& clipName, EActionPhase phase) -> bool
        {
            if (clipName.empty() || !anim->HasClip(clipName))
                return false;

            const std::string locomotionClip = ResolveSafeLocomotionClip();
            if (!locomotionClip.empty() && anim->HasClip(locomotionClip))
                StartLocomotionClip(locomotionClip);

            if (!anim->PlayUpperBodyOverlay(clipName, false, 0.0f, 0.12f))
                return false;

            m_actionPhase = phase;
            return true;
        };

    auto StartFullBodyAction = [&](const std::string& clipName, EActionPhase phase, float blendTimeSec) -> bool
        {
            if (clipName.empty() || !anim->HasClip(clipName))
                return false;

            anim->StopUpperBodyOverlay(true);

            if (!anim->GetCurrentClipName().empty())
            {
                if (!anim->CrossFade(clipName, blendTimeSec, false, 0.0f))
                    anim->Play(clipName, false, 0.0f);
            }
            else
            {
                anim->Play(clipName, false, 0.0f);
            }

            m_actionPhase = phase;
            m_state = EAnimState::Attack;
            return true;
        };

    if (m_attackQueued && m_actionPhase == EActionPhase::None)
    {
        m_attackQueued = false;

        EActionPhase nextPhase = EActionPhase::None;
        const std::string atkClip = ResolveAttackStartClip(nextPhase);

        if (!atkClip.empty() && anim->HasClip(atkClip))
        {
            if (IsOverlayActionPhase(nextPhase))
            {
                StartUpperBodyAttack(atkClip, nextPhase);
            }
            else
            {
                if (StartFullBodyAction(atkClip, nextPhase, 0.12f))
                {
                    animPrevState = m_state;
                    return;
                }
            }
        }
    }

    if (m_hitQueued && m_actionPhase == EActionPhase::None)
    {
        m_hitQueued = false;

        const std::string hitClip = ResolveHitClip();
        if (!hitClip.empty() && anim->HasClip(hitClip))
        {
            anim->StopUpperBodyOverlay(true);

            if (!anim->GetCurrentClipName().empty())
            {
                if (!anim->CrossFade(hitClip, 0.08f, false, 0.0f))
                    anim->Play(hitClip, false, 0.0f);
            }
            else
            {
                anim->Play(hitClip, false, 0.0f);
            }

            m_actionPhase = EActionPhase::Hit;
            animPrevState = m_state;
            return;
        }
    }

    if (m_actionPhase != EActionPhase::None)
    {
        const bool overlayAction = IsOverlayActionPhase(m_actionPhase);
        const bool actionFinished = overlayAction ? anim->IsUpperBodyOverlayFinished() : anim->IsCurrentClipFinished();

        if (actionFinished)
        {
            if (m_actionPhase == EActionPhase::AttackBowLoad)
            {
                if (anim->HasClip("Bow_Release"))
                {
                    if (anim->PlayUpperBodyOverlay("Bow_Release", false, 0.0f, 0.05f))
                        m_actionPhase = EActionPhase::AttackBowRelease;
                    else
                    {
                        anim->StopUpperBodyOverlay();
                        m_actionPhase = EActionPhase::None;
                    }
                }
                else
                {
                    anim->StopUpperBodyOverlay();
                    m_actionPhase = EActionPhase::None;
                }
            }
            else
            {
                if (overlayAction)
                    anim->StopUpperBodyOverlay();

                const bool wantsMove = WantsMoveNow();
                const EAnimState targetState = wantsMove ? EAnimState::Move : EAnimState::Idle;
                std::string targetClip = ResolveLocomotionClip(targetState);

                if (targetClip.empty() || !anim->HasClip(targetClip))
                    targetClip = ResolveIdleClip();

                if (!targetClip.empty() && anim->HasClip(targetClip))
                {
                    if (!overlayAction)
                    {
                        if (!anim->CrossFade(targetClip, 0.12f, true, 0.0f))
                            anim->Play(targetClip, true, 0.0f);
                    }
                    else if (anim->GetCurrentClipName().empty())
                    {
                        anim->Play(targetClip, true, 0.0f);
                    }
                }

                m_actionPhase = EActionPhase::None;
                m_state = targetState;
            }
        }

        if (!overlayAction)
        {
            animPrevState = m_state;
            return;
        }
    }

    const bool wantsMove = WantsMoveNow();
    const EAnimState targetState = wantsMove ? EAnimState::Move : EAnimState::Idle;

    std::string targetClip = ResolveLocomotionClip(targetState);
    if (targetClip.empty() || !anim->HasClip(targetClip))
    {
        targetClip = ResolveIdleClip();
        if (targetClip.empty() || !anim->HasClip(targetClip))
        {
            animPrevState = m_state;
            return;
        }
    }

    constexpr float kBlendTime = 0.15f;

    if (anim->GetCurrentClipName().empty())
    {
        anim->Play(targetClip, true, 0.0f);
    }
    else if (anim->GetCurrentClipName() != targetClip)
    {
        if (!anim->CrossFade(targetClip, kBlendTime, true, 0.0f))
            anim->Play(targetClip, true, 0.0f);
    }

    if (m_actionPhase == EActionPhase::None)
        m_state = targetState;

    animPrevState = m_state;
}

bool CAnimController::IsActionLocked() const
{
    if (m_actionPhase == EActionPhase::None)
        return false;

    // Hit은 그대로 잠금 유지
    if (m_actionPhase == EActionPhase::Hit)
        return true;

    const EWeaponType weapon = GetEquippedWeaponType(m_pOwner);

    switch (m_actionPhase)
    {
    case EActionPhase::AttackBowLoad:
    case EActionPhase::AttackBowRelease:
        // 활은 공격 중 이동/회전 허용
        return false;

    case EActionPhase::AttackGeneric:
        // AttackGeneric은 Sword / Axe / Gun에서 사용
        switch (weapon)
        {
        case EWeaponType::Gun:
            return false; // 총은 공격 중 이동/회전 허용

        case EWeaponType::Sword:
        case EWeaponType::Axe:
        case EWeaponType::None:
        default:
            return true;  // 검/도끼/기타는 기존처럼 잠금
        }

    default:
        return true;
    }
}

bool CAnimController::RequestAttack()
{
    if (m_actionPhase != EActionPhase::None)
        return false;

    EActionPhase phase = EActionPhase::None;
    const std::string atkClip = ResolveAttackStartClip(phase);

    if (atkClip.empty())
        return false;

    m_attackQueued = true;
    return true;
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