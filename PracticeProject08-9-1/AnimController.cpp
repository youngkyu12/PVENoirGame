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

	static bool IsLocomotionClipName(const std::string& clipName)
	{
		if ( clipName.empty() )
			return false;

		return
			( clipName.rfind("Walk_", 0) == 0 ) ||
			( clipName.rfind("Run_", 0) == 0 );
	}

	static float ComputeLocomotionPhaseMatchedStartTime(CAnimator* anim, const std::string& targetClip)
	{
		if ( !anim )
			return 0.0f;

		const std::string& currentClip = anim->GetCurrentClipName();

		if ( !IsLocomotionClipName(currentClip) )
			return 0.0f;

		if ( !IsLocomotionClipName(targetClip) )
			return 0.0f;

		const float currentDuration = anim->GetCurrentClipDuration();
		const float targetDuration = anim->GetClipDuration(targetClip);

		if ( currentDuration <= 1e-6f || targetDuration <= 1e-6f )
			return 0.0f;

		float normalizedTime = anim->GetCurrentTime() / currentDuration;

		if ( normalizedTime < 0.0f ) normalizedTime = 0.0f;
		if ( normalizedTime > 1.0f ) normalizedTime = 1.0f;

		return normalizedTime * targetDuration;
	}

	static void StartLocomotionClipPreservePhase(CAnimator* anim, const std::string& targetClip, float blendTimeSec)
	{
		if ( !anim )
			return;

		if ( targetClip.empty() || !anim->HasClip(targetClip) )
			return;

		const std::string& currentClip = anim->GetCurrentClipName();

		if ( currentClip.empty() )
		{
			anim->Play(targetClip, true, 0.0f);
			return;
		}

		if ( currentClip == targetClip )
			return;

		const float startTime = ComputeLocomotionPhaseMatchedStartTime(anim, targetClip);

		if ( !anim->CrossFade(targetClip, blendTimeSec, true, startTime) )
			anim->Play(targetClip, true, startTime);
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

std::string CAnimController::ResolveDeathClip() const
{
	if ( !m_usePlayerClipSet )
		return m_deathClip;

	return "Death";
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
	LocalUpdate(dt);
#else
	LocalUpdate(dt);
#endif

   
    animPrevState = m_state;
}

void CAnimController::NetworkUpdate(float dt)
{
    (void)dt;

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

    auto ResolveSafeLocomotionClipFromState = [&](EAnimState state) -> std::string
        {
            std::string clip = ResolveLocomotionClip(state);

            if (state == EAnimState::Move && (clip.empty() || !anim->HasClip(clip)))
                clip = m_moveClip;

            if (clip.empty() || !anim->HasClip(clip))
                clip = ResolveIdleClip();

            return clip;
        };

    auto StartUpperBodyAttack = [&](const std::string& clipName, EActionPhase phase, EAnimState baseState) -> bool
        {
            if (clipName.empty() || !anim->HasClip(clipName))
                return false;

            const std::string locomotionClip = ResolveSafeLocomotionClipFromState(baseState);
            if (!locomotionClip.empty() && anim->HasClip(locomotionClip))
                StartLocomotionClip(locomotionClip);

            if (!anim->PlayUpperBodyOverlay(clipName, false, 0.0f, 0.12f))
                return false;

            m_actionPhase = phase;
            return true;
        };

    auto StartFullBodyAction = [&](const std::string& clipName, EActionPhase phase, float blendTimeSec, float startTime) -> bool
        {
            if (clipName.empty() || !anim->HasClip(clipName))
                return false;

            anim->StopUpperBodyOverlay(true);
            anim->ClearVisualYawOffset();

            if (!anim->GetCurrentClipName().empty())
            {
                if (!anim->CrossFade(clipName, blendTimeSec, false, startTime))
                    anim->Play(clipName, false, startTime);
            }
            else
            {
                anim->Play(clipName, false, startTime);
            }

            m_actionPhase = phase;
            return true;
        };

    auto StartFullBodyRoll = [&](const std::string& clipName, float visualYawDeg, float startTime) -> bool
        {
            if (clipName.empty() || !anim->HasClip(clipName))
                return false;

            anim->StopUpperBodyOverlay(true);
            anim->ClearVisualYawOffset();
            anim->Play(clipName, false, startTime);

            anim->SetVisualYawOffset(
                visualYawDeg,
                3.0f / 45.0f,
                42.0f / 45.0f
            );

            m_actionPhase = EActionPhase::Roll;
            m_state = EAnimState::Attack;
            return true;
        };

    // 서버 권위 경로에서는 로컬 공격 큐는 사용하지 않는다.
    //m_attackQueued = false;

    // Protocol Roll 이벤트 우선 처리
    if (m_rollQueued && m_actionPhase == EActionPhase::None)
    {
        m_rollQueued = false;

        std::string rollClip = m_rollQueuedClipName;
        float visualYawDeg = m_rollQueuedVisualYawDeg;

        if (rollClip.empty() || !anim->HasClip(rollClip))
            rollClip = ResolveRollClip(m_rollMoveDirBits, visualYawDeg);

        if (StartFullBodyRoll(rollClip, visualYawDeg, m_startTime))
            return;
    }

    // ------------------------------------------------------------
    // State change (network authoritative)
    // ------------------------------------------------------------
   // if (m_state != animPrevState)
   // {
   //     if (m_state == EAnimState::Attack)
   //     {
   //         EActionPhase nextPhase = EActionPhase::None;
   //         std::string atkClip = ResolveAttackStartClip(nextPhase);

   //         if (atkClip.empty() || !anim->HasClip(atkClip))
   //         {
   //             atkClip = m_attackClip;
   //             nextPhase = EActionPhase::AttackGeneric;
   //         }

   //         if (!atkClip.empty() && anim->HasClip(atkClip))
   //         {
   //             const bool overlayAttack = IsOverlayActionPhase(nextPhase);
   //             const EAnimState baseState = (animPrevState == EAnimState::Move) ? EAnimState::Move : EAnimState::Idle;

   //             if (overlayAttack)
   //             {
   //                 StartUpperBodyAttack(atkClip, nextPhase, baseState);
   //             }
   //             else
   //             {
   //                 constexpr float kBlendTime = 0.15f;
   //                 StartFullBodyAction(atkClip, nextPhase, kBlendTime, m_startTime);
   //             }
   //         }
   //         return;
   //     }

   //     std::string targetClip = ResolveSafeLocomotionClipFromState(m_state);
   //     if (!targetClip.empty() && anim->HasClip(targetClip))
   //     {
   //         constexpr float kBlendTime = 0.15f;

   //         //if (!anim->CrossFade(targetClip, kBlendTime, true, 0.0f))
   //         //    anim->Play(targetClip, true, 0.0f);
			////	이현석: 이동 상태 전환 시 normalized time을 새 클립 duration에 맞게 환산해서 넘김
			//StartLocomotionClipPreservePhase(anim, targetClip, kBlendTime);
   //     }
   // }

    if (m_attackQueued && m_actionPhase == EActionPhase::None)
    {
        m_attackQueued = false;

        EActionPhase nextPhase = EActionPhase::None;
        const std::string atkClip = ResolveAttackStartClip(nextPhase);

        if (!atkClip.empty() && anim->HasClip(atkClip))
        {
            if (IsOverlayActionPhase(nextPhase))
            {
                StartUpperBodyAttack(atkClip, nextPhase, m_state);
            }
            else
            {
                if (StartFullBodyAction(atkClip, nextPhase, 0.15f, 0.12f))
                {
                    animPrevState = m_state;
                    return;
                }
            }
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
            anim->StopUpperBodyOverlay(true);
            anim->ClearVisualYawOffset();

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
        const bool overlayAction = IsOverlayActionPhase(m_actionPhase);
        const bool actionFinished = overlayAction ? anim->IsUpperBodyOverlayFinished() : anim->IsCurrentClipFinished();

        if (actionFinished)
        {
            if (m_actionPhase == EActionPhase::AttackBowLoad)
            {
                if (anim->HasClip("Bow_Release"))
                {
                    if (overlayAction)
                    {
                        if (anim->PlayUpperBodyOverlay("Bow_Release", false, 0.0f, 0.05f))
                        {
                            m_actionPhase = EActionPhase::AttackBowRelease;
                            return;
                        }

                        anim->StopUpperBodyOverlay();
                    }
                    else
                    {
                        constexpr float kBowChainBlendTime = 0.05f;

                        if (!anim->CrossFade("Bow_Release", kBowChainBlendTime, false, 0.0f))
                            anim->Play("Bow_Release", false, 0.0f);

                        m_actionPhase = EActionPhase::AttackBowRelease;
                        return;
                    }
                }
            }

            if (overlayAction)
                anim->StopUpperBodyOverlay();

            if (m_actionPhase == EActionPhase::Roll)
                anim->ClearVisualYawOffset();

            const EAnimState targetState =
                (m_state == EAnimState::Move) ? EAnimState::Move : EAnimState::Idle;

            std::string targetClip = ResolveSafeLocomotionClipFromState(targetState);

            if (!targetClip.empty() && anim->HasClip(targetClip))
            {
                if (!overlayAction)
                {
                    constexpr float kOutBlendTime = 0.12f;

                    if (!anim->CrossFade(targetClip, kOutBlendTime, true, 0.0f))
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

        if (m_actionPhase != EActionPhase::None && !IsOverlayActionPhase(m_actionPhase))
            return;
    }

    // ------------------------------------------------------------
    // Keep locomotion in sync with network state
    // ------------------------------------------------------------
    std::string targetClip = ResolveSafeLocomotionClipFromState(m_state);

    if (targetClip.empty() || !anim->HasClip(targetClip))
        return;

    constexpr float kBlendTime = 0.15f;

    if (anim->GetCurrentClipName().empty())
    {
        anim->Play(targetClip, true, 0.0f);
    }
    else if (anim->GetCurrentClipName() != targetClip)
    {
        if (!anim->CrossFade(targetClip, kBlendTime, true, 0.0f))
            anim->Play(targetClip, true, 0.0f);
		//	이현석: 이동 상태 전환 시 normalized time을 새 클립 duration에 맞게 환산해서 넘김
		//	if (!anim->CrossFade(targetClip, kBlendTime, true, 0.0f))
		//		anim->Play(targetClip, true, 0.0f);
		//	위의 2줄 지우고 밑으로 교체
		StartLocomotionClipPreservePhase(anim, targetClip, kBlendTime);

    }
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

	auto StartLocomotionClip = [ & ] (const std::string& clipName)
		{
			constexpr float kBlendTime = 0.15f;
			StartLocomotionClipPreservePhase(anim, clipName, kBlendTime);
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

	auto StartDeathAction = [ & ] () -> bool
		{
			const std::string deathClip = ResolveDeathClip();

			if ( deathClip.empty() || !anim->HasClip(deathClip) )
				return false;

			anim->StopUpperBodyOverlay(true);
			anim->ClearVisualYawOffset();
			anim->Play(deathClip, false, 0.0f);

			m_attackQueued = false;
			m_hitQueued = false;
			m_rollQueued = false;
			m_deathQueued = false;

			m_speed = 0.0f;
			m_moveDirBits = 0;
			m_bRunRequested = false;

			m_actionPhase = EActionPhase::Death;
			m_state = EAnimState::Die;
			return true;
		};

	if ( m_actionPhase == EActionPhase::Death )
	{
		animPrevState = m_state;
		return;
	}

	if ( m_deathQueued )
	{
		if ( StartDeathAction() )
		{
			animPrevState = m_state;
			return;
		}

		m_deathQueued = false;
	}

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
	StartLocomotionClipPreservePhase(anim, targetClip, kBlendTime);

	if ( m_actionPhase == EActionPhase::None )
		m_state = targetState;

}

bool CAnimController::IsRunLocomotionActive() const
{
	if ( !m_pOwner )
		return false;

	if ( !m_usePlayerClipSet )
		return false;

	// 구르기, 피격, 사망, 공격 등 액션 중에는
	// Shift + WASD 입력이 들어와도 AI 감지용 "달리기"로 취급하지 않는다.
	if ( m_actionPhase != EActionPhase::None )
		return false;

	const uint32_t horizontalDirBits =
		m_moveDirBits & ( DIR_FORWARD | DIR_BACKWARD | DIR_LEFT | DIR_RIGHT );

	if ( horizontalDirBits == 0 )
		return false;

	if ( !m_bRunRequested )
		return false;

	if ( m_state != EAnimState::Move )
		return false;

	CAnimator* anim = nullptr;

	if ( auto* animComp = m_pOwner->GetComponent<CAnimatorComponent>() )
		anim = animComp->GetAnimator();

	if ( !anim )
		anim = m_pOwner->GetAnimator();

	if ( !anim )
		return false;

	const std::string& currentClip = anim->GetCurrentClipName();

	// 실제 Run_* locomotion clip이 현재 재생 중일 때만 true.
	return currentClip.rfind("Run_", 0) == 0;
}

bool CAnimController::IsActionLocked() const
{
	if ( m_actionPhase == EActionPhase::None )
		return false;

	if ( m_actionPhase == EActionPhase::Death )
		return true;

	if ( m_actionPhase == EActionPhase::Hit )
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

bool CAnimController::RequestDeath()
{
	if ( m_actionPhase == EActionPhase::Death )
		return false;

	m_attackQueued = false;
	m_hitQueued = false;
	m_rollQueued = false;

	m_speed = 0.0f;
	m_moveDirBits = 0;
	m_bRunRequested = false;

	m_deathQueued = true;
	return true;
}

void CAnimController::ResetToIdleAfterRespawn()
{
	m_attackQueued = false;
	m_hitQueued = false;
	m_rollQueued = false;
	m_deathQueued = false;

	m_speed = 0.0f;
	m_moveDirBits = 0;
	m_bRunRequested = false;

	m_actionPhase = EActionPhase::None;
	m_state = EAnimState::Idle;
	animPrevState = EAnimState::Idle;

	CAnimator* anim = nullptr;

	if ( m_pOwner )
	{
		if ( auto* animComp = m_pOwner->GetComponent<CAnimatorComponent>() )
			anim = animComp->GetAnimator();

		if ( !anim )
			anim = m_pOwner->GetAnimator();
	}

	if ( !anim )
		return;

	anim->StopUpperBodyOverlay(true);
	anim->ClearVisualYawOffset();

	const std::string idleClip = ResolveIdleClip();

	if ( !idleClip.empty() && anim->HasClip(idleClip) )
	{
		anim->Play(idleClip, true, 0.0f);
	}
}

bool CAnimController::IsMeleeAttackHitboxActive() const
{
	// 핵심:
	// Roll / Hit / Bow_Load / Bow_Release / Gun_Shoot 는 모두 제외
	if ( m_actionPhase != EActionPhase::AttackGeneric )
		return false;

	switch ( GetEquippedWeaponType(m_pOwner) )
	{
	case EWeaponType::Sword:
	case EWeaponType::Axe:
		return true;

	case EWeaponType::Bow:
	case EWeaponType::Gun:
	case EWeaponType::None:
	default:
		return false;
	}
}