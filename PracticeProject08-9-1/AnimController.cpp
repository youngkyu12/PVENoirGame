//-----------------------------------------------------------------------------
// File: AnimController.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "AnimController.h"
#include "AnimatorComponent.h"
#include "Object.h"
#include "Animator.h"

void CAnimController::Update(float /*dt*/)
{
    if (!m_pOwner) return;

    CAnimator* anim = nullptr;

    if (auto* animComp = m_pOwner->GetComponent<CAnimatorComponent>())
        anim = animComp->GetAnimator();

    if (!anim)
        anim = m_pOwner->GetAnimator();

    // ------------------------------------------------------------
    // 0) Attack queued ó�� (���� Ŭ�� ����)
    // ------------------------------------------------------------
    if (m_attackQueued)
    {
        m_attackQueued = false;

        const char* atk = m_attackClip.c_str();
        if (atk && anim->HasClip(atk))
        {
            // Attack ���Ե� ������ ����
            constexpr float kAtkBlendTime = 0.12f;

            // ���� ��� ���̸� CrossFade, �ƴϸ� Play
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
    // 1) Attack ���¸�: ���� ������ ����, ������ Idle/Move�� ����
    // ------------------------------------------------------------
    if (m_state == EAnimState::Attack)
    {
        // ���� Ŭ���� �������� (non-loop finished) -> �Է� �ӵ��� ���� ����
        if (anim->IsCurrentClipFinished())
        {
            // ���⼭ "�̵�Ű �Է����̸� Run" ������ speed�� ���� ����
            // (speed > eps)�� Move, �ƴϸ� Idle
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
        return; // Attack �߿��� Idle/Move ���� ���� ����
    }

    // ------------------------------------------------------------
    // 2) �⺻ Idle/Move ���¸ӽ�
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

