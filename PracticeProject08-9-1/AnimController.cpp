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
    static float Clamp01(float v)
    {
        if (v < 0.0f) return 0.0f;
        if (v > 1.0f) return 1.0f;
        return v;
    }

    static XMFLOAT3 BuildRollMoveDirection(const CGameObject* owner, uint32_t dirBits)
    {
        if (!owner)
            return XMFLOAT3(0.0f, 0.0f, 1.0f);

        XMFLOAT3 look = owner->GetLook();
        XMFLOAT3 right = owner->GetRight();

        look.y = 0.0f;
        right.y = 0.0f;

        XMVECTOR lookV = XMLoadFloat3(&look);
        XMVECTOR rightV = XMLoadFloat3(&right);

        if (XMVectorGetX(XMVector3LengthSq(lookV)) <= 1e-8f)
            lookV = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        else
            lookV = XMVector3Normalize(lookV);

        if (XMVectorGetX(XMVector3LengthSq(rightV)) <= 1e-8f)
            rightV = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
        else
            rightV = XMVector3Normalize(rightV);

        XMVECTOR moveV = XMVectorZero();

        if (dirBits & DIR_FORWARD)  moveV += lookV;
        if (dirBits & DIR_BACKWARD) moveV -= lookV;
        if (dirBits & DIR_RIGHT)    moveV += rightV;
        if (dirBits & DIR_LEFT)     moveV -= rightV;

        if (XMVectorGetX(XMVector3LengthSq(moveV)) <= 1e-8f)
            moveV = lookV;
        else
            moveV = XMVector3Normalize(moveV);

        XMFLOAT3 out{};
        XMStoreFloat3(&out, moveV);
        return out;
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

std::string CAnimController::ResolveRollClip(uint32_t dirBits, float& outVisualYawDeg) const
{
    outVisualYawDeg = 0.0f;

    if (!m_usePlayerClipSet)
        return "";

    const std::string suffix = BuildDirectionSuffix(dirBits);

    if (suffix == "L")
    {
        outVisualYawDeg = -90.0f;
        return "Roll_F";
    }

    if (suffix == "FL")
    {
        outVisualYawDeg = -45.0f;
        return "Roll_F";
    }

    if (suffix == "F" || suffix.empty())
    {
        outVisualYawDeg = 0.0f;
        return "Roll_F";
    }

    if (suffix == "FR")
    {
        outVisualYawDeg = 45.0f;
        return "Roll_F";
    }

    if (suffix == "R")
    {
        outVisualYawDeg = 90.0f;
        return "Roll_F";
    }

    if (suffix == "BL")
    {
        outVisualYawDeg = 45.0f;
        return "Roll_B";
    }

    if (suffix == "B")
    {
        outVisualYawDeg = 0.0f;
        return "Roll_B";
    }

    if (suffix == "BR")
    {
        outVisualYawDeg = -45.0f;
        return "Roll_B";
    }

    outVisualYawDeg = 0.0f;
    return "Roll_F";
}

std::string CAnimController::ResolveLocomotionClip(EAnimState state) const
{
    if (state == EAnimState::Move)
        return ResolveMoveClip();

    return ResolveIdleClip();
}

void CAnimController::Update(float dt)
{
    if (!m_pOwner) return;

#ifdef USING_NETWORK
	NetworkUpdate(dt);
#else
	LocalUpdate(dt);
#endif

   
    animPrevState = m_state;
}

void CAnimController::NetworkUpdate(float dt)
{
    CAnimator* anim = nullptr;

    if (auto* animComp = m_pOwner->GetComponent<CAnimatorComponent>())
        anim = animComp->GetAnimator();

    if (!anim)
        anim = m_pOwner->GetAnimator();

    if (!anim) return;

    // ------------------------------------------------------------
    // Attack request
    // ------------------------------------------------------------

    constexpr float kBlendTime = 0.15f;

    if (m_state != animPrevState)
    {
        if (m_state == EAnimState::Attack)
        {
            EActionPhase nextPhase = EActionPhase::None;
            std::string atkClip = ResolveAttackStartClip(nextPhase);

            if (atkClip.empty() || !anim->HasClip(atkClip))
                atkClip = m_attackClip;

            if (!atkClip.empty() && anim->HasClip(atkClip))
            {
                if (!anim->CrossFade(atkClip, kBlendTime, false, m_startTime))
                    anim->Play(atkClip, false, m_startTime);

                m_actionPhase = nextPhase;
            }
            return;
        }

        std::string targetClip = ResolveLocomotionClip(m_state);
        if (targetClip.empty() || !anim->HasClip(targetClip))
            targetClip = ResolveIdleClip();

        if (!targetClip.empty() && anim->HasClip(targetClip))
        {
            if (!anim->CrossFade(targetClip, kBlendTime, true, 0.0f))
                anim->Play(targetClip, true, 0.0f);
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

            const EAnimState targetState =
                (m_state == EAnimState::Move) ? EAnimState::Move : EAnimState::Idle;


            std::string targetClip = ResolveLocomotionClip(targetState);

            if (targetState == EAnimState::Move && (targetClip.empty() || !anim->HasClip(targetClip)))
                targetClip = m_moveClip; // 네트워크에서는 방향비트 의존 최소화

            if (targetClip.empty() || !anim->HasClip(targetClip))
                targetClip = ResolveIdleClip();

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
    std::string targetClip;

    if (m_state == EAnimState::Move)
    {
        targetClip = m_moveClip;
        if ((targetClip.empty() || !anim->HasClip(targetClip)) && m_usePlayerClipSet)
            targetClip = "Walk_F";
    }
    else
    {
        targetClip = ResolveIdleClip();
    }

    if (targetClip.empty() || !anim->HasClip(targetClip))
    {
        targetClip = ResolveIdleClip();
        if (targetClip.empty() || !anim->HasClip(targetClip))
            return;
    }

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
    EAnimState target = (m_speed > m_moveEps) ? EAnimState::Move : EAnimState::Idle;

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

}


void CAnimController::LocalUpdate(float dt)
{
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
            anim->ClearVisualYawOffset();

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
    auto StartFullBodyRoll = [&](const std::string& clipName, float visualYawDeg) -> bool
        {
            if (clipName.empty() || !anim->HasClip(clipName))
                return false;

            anim->StopUpperBodyOverlay(true);
            anim->ClearVisualYawOffset();

            // 구르기는 즉시 시작한다. CrossFade를 사용하지 않는다.
            anim->Play(clipName, false, 0.0f);

            // 45프레임 기준:
            // 0~3프레임 동안 회전, 42~45프레임 동안 원복
            anim->SetVisualYawOffset(
                visualYawDeg,
                3.0f / 45.0f,
                42.0f / 45.0f
            );

            m_actionPhase = EActionPhase::Roll;
            m_state = EAnimState::Attack;
            return true;
        };

    if (m_rollQueued && m_actionPhase == EActionPhase::None)
    {
        m_rollQueued = false;

        if (StartFullBodyRoll(m_rollQueuedClipName, m_rollQueuedVisualYawDeg))
        {
            animPrevState = m_state;
            return;
        }
    }

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
            anim->ClearVisualYawOffset();

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
                if (m_actionPhase == EActionPhase::Roll)
                    anim->ClearVisualYawOffset();

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
        if ((m_actionPhase == EActionPhase::Roll) && !actionFinished)
        {
            const float clipDuration = anim->GetCurrentClipDuration();

            if ((clipDuration > 1e-6f) && (m_rollMoveSpeed != 0.0f))
            {
                float startN = Clamp01(m_rollMoveStartNormalized);
                float endN = Clamp01(m_rollMoveEndNormalized);

                if (endN < startN)
                {
                    const float t = startN;
                    startN = endN;
                    endN = t;
                }

                const float curTime = anim->GetCurrentTime();
                float nextTime = curTime + dt;
                if (nextTime > clipDuration)
                    nextTime = clipDuration;

                const float moveStartTime = clipDuration * startN;
                const float moveEndTime = clipDuration * endN;

                const float activeBegin = (curTime > moveStartTime) ? curTime : moveStartTime;
                const float activeEnd = (nextTime < moveEndTime) ? nextTime : moveEndTime;
                const float activeDt = activeEnd - activeBegin;

                if (activeDt > 0.0f)
                {
                    const XMFLOAT3 moveDir = BuildRollMoveDirection(m_pOwner, m_rollMoveDirBits);

                    if (auto* tr = m_pOwner->GetComponent<CTransformComponent>())
                    {
                        tr->Translate(XMFLOAT3(
                            moveDir.x * m_rollMoveSpeed * activeDt,
                            moveDir.y * m_rollMoveSpeed * activeDt,
                            moveDir.z * m_rollMoveSpeed * activeDt
                        ));
                    }
                }
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
    case EActionPhase::Roll:
        return true;
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

bool CAnimController::RequestRoll(uint32_t dirBits)
{
    if (m_actionPhase != EActionPhase::None)
        return false;

    const uint32_t horizontalDirBits =
        dirBits & (DIR_FORWARD | DIR_BACKWARD | DIR_LEFT | DIR_RIGHT);

    float visualYawDeg = 0.0f;
    const std::string rollClip = ResolveRollClip(horizontalDirBits, visualYawDeg);

    if (rollClip.empty())
        return false;

    m_rollQueuedClipName = rollClip;
    m_rollQueuedVisualYawDeg = visualYawDeg;
    m_rollMoveDirBits = (horizontalDirBits != 0) ? horizontalDirBits : DIR_FORWARD;
    m_rollQueued = true;

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