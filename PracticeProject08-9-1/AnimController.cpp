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

    CAnimator* anim = m_pOwner->EnsureAnimator();
    if (!anim) return;

    // 1) target state 결정 (Idle/Move only)
    EAnimState target = (m_speed > m_moveEps) ? EAnimState::Move : EAnimState::Idle;

    // 2) target clip 결정 + 유효성 확인
    const char* targetClip = ClipFor(target);

    if (!anim->HasClip(targetClip))
    {
        // fallback to Idle
        target = EAnimState::Idle;
        targetClip = "Idle";
        if (!anim->HasClip(targetClip)) return;
    }

    // 3) "상태가 바뀔 때만" 전환 트리거
    //    (GetCurrentClipName() 기준으로 비교하면 블렌딩 중에 계속 재요청되어 망가질 수 있음)
    if (target != m_state)
    {
        // blendTime은 컨트롤러 파라미터로 빼도 되지만, 최소 패치로 상수 사용
        constexpr float kBlendTime = 0.15f;

        // CrossFade가 실패(클립 없음 등)하면 Play로라도 전환
        if (!anim->CrossFade(targetClip, kBlendTime, true, 0.0f))
        {
            anim->Play(targetClip, true, 0.0f);
        }

        m_state = target;
    }
    else
    {
        // 상태 유지
        m_state = target;
    }

    if (anim->GetCurrentClipName().empty())
    {
        // 시작 시 1회는 무조건 재생 시작
        anim->Play(targetClip, true, 0.0f);
        m_state = target;
        return;
    }

}
