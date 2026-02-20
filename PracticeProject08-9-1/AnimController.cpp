//-----------------------------------------------------------------------------
// File: AnimController.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "AnimController.h"
#include "Object.h"
#include "Animator.h"

void CAnimController::Update(float /*dt*/)
{
    if (!m_pOwner) return;

    CAnimator* anim = m_pOwner->GetAnimator();
    if (!anim) return;

    // ------------------------------------------------------------
    // 0) Attack queued 처리 (연속 클릭 무시)
    // ------------------------------------------------------------
    if (m_attackQueued)
    {
        m_attackQueued = false;

        const char* atk = m_attackClip.c_str();
        if (atk && anim->HasClip(atk))
        {
            // Attack 진입도 블렌딩 적용
            constexpr float kAtkBlendTime = 0.12f;

            // 현재 재생 중이면 CrossFade, 아니면 Play
            if (!anim->GetCurrentClipName().empty())
            {
                if (!anim->CrossFade(atk, kAtkBlendTime, false, 0.0f))
                    anim->Play(atk, false, 0.0f);
            }
            else
            {
                anim->Play(atk, false, 0.0f);
            }

            m_state = EAnimState::Attack;
            return;
        }
    }


    // ------------------------------------------------------------
    // 1) Attack 상태면: 끝날 때까지 유지, 끝나면 Idle/Move로 복귀
    // ------------------------------------------------------------
    if (m_state == EAnimState::Attack)
    {
        // 공격 클립이 끝났으면 (non-loop finished) -> 입력 속도에 따라 복귀
        if (anim->IsCurrentClipFinished())
        {
            // 여기서 "이동키 입력중이면 Run" 판정은 speed로 간접 판정
            // (speed > eps)면 Move, 아니면 Idle
            const EAnimState target = (m_speed > m_moveEps) ? EAnimState::Move : EAnimState::Idle;
            const char* targetClip = ClipFor(target);

            if (!anim->HasClip(targetClip))
            {
                targetClip = "Idle";
                if (!anim->HasClip(targetClip)) return;
            }

            constexpr float kOutBlendTime = 0.12f;
            if (!anim->CrossFade(targetClip, kOutBlendTime, true, 0.0f))
                anim->Play(targetClip, true, 0.0f);

            m_state = target;

        }
        return; // Attack 중에는 Idle/Move 로직 진입 금지
    }

    // ------------------------------------------------------------
    // 2) 기본 Idle/Move 상태머신
    // ------------------------------------------------------------
    EAnimState target = (m_speed > m_moveEps) ? EAnimState::Move : EAnimState::Idle;

    const char* targetClip = ClipFor(target);

    if (!anim->HasClip(targetClip))
    {
        target = EAnimState::Idle;
        targetClip = "Idle";
        if (!anim->HasClip(targetClip)) return;
    }

    if (target != m_state)
    {
        constexpr float kBlendTime = 0.15f;

        if (!anim->CrossFade(targetClip, kBlendTime, true, 0.0f))
        {
            anim->Play(targetClip, true, 0.0f);
        }

        m_state = target;
    }
    else
    {
        m_state = target;
    }

    if (anim->GetCurrentClipName().empty())
    {
        anim->Play(targetClip, true, 0.0f);
        m_state = target;
        return;
    }
}


void CAnimController::RequestAttack()
{
    if (m_state == EAnimState::Attack) return;

    m_attackQueued = true;
}

