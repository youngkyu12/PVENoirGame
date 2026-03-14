//-----------------------------------------------------------------------------
// File: AnimController.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "AnimController.h"
#include "AnimatorComponent.h"
#include "Object.h"
#include "Animator.h"

#include "GlobalEnum.h"
#include "GlobalValues.h"

void CAnimController::Update(float /*dt*/)
{
    if (!m_pOwner) return;

    CAnimator* anim = nullptr;

    if (auto* animComp = m_pOwner->GetComponent<CAnimatorComponent>())
        anim = animComp->GetAnimator();

    if (!anim)
        anim = m_pOwner->GetAnimator();

#ifdef USING_NETWORK

    // 데모: play만 열심히 하자
    constexpr float kBlendTime = 0.15f;

    if (m_state != animPrevState) {
        const char* targetClip = ClipFor(m_state);


        if(m_state == EAnimState::Attack)
        {
            if (!anim->CrossFade(m_attackClip, kBlendTime, false, m_startTime))
            {
                anim->Play(m_attackClip, false, m_startTime);
            }
            return;
		}


        if (!anim->CrossFade(targetClip, kBlendTime, true, 0.0f))
        {
            
            anim->Play(targetClip, true, m_startTime);
        }
    }

	   

#else
    if (m_attackQueued)
    {
        m_attackQueued = false;

        const char* atk = m_attackClip.c_str();
        if (atk && anim->HasClip(atk))
        {
            constexpr float kAtkBlendTime = 0.12f;

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

    if (m_state == EAnimState::Attack)
    {
        if (anim->IsCurrentClipFinished())
        {
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
        return;
    }

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
#endif

    animPrevState = m_state;
}


void CAnimController::RequestAttack()
{
    if (m_state == EAnimState::Attack) return;

    m_attackQueued = true;
}

