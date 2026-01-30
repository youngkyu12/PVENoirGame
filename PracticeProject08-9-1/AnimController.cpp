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

    // 2) 상태 변경(혹은 유지) -> 필요한 경우에만 Play
    const char* targetClip = ClipFor(target);

    // 클립 없으면 안전하게 Idle로 fallback
    if (!anim->HasClip(targetClip))
    {
        target = EAnimState::Idle;
        targetClip = "Idle";
        if (!anim->HasClip(targetClip)) return; // Idle도 없으면 포기
    }

    // 중복 Play 방지 (중요)
    if (anim->GetCurrentClipName() != targetClip)
    {
        anim->Play(targetClip, true, 0.0f); // loop=true, start=0
        m_state = target;
    }
    else
    {
        m_state = target;
    }
}
