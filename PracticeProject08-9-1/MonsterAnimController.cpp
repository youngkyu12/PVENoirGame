//------------------------------------------------------- ----------------------
// File: MonsterAnimController.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "MonsterAnimController.h"

#include "Object.h"
#include "Animator.h"
#include "AnimatorComponent.h"

CAnimator* CMonsterAnimController::ResolveAnimator() const
{
	if ( !m_pOwner ) return nullptr;

	if ( auto* animComp = m_pOwner->GetComponent<CAnimatorComponent>() )
		return animComp->EnsureAnimator();

	return m_pOwner->GetAnimator();
}

std::string CMonsterAnimController::ResolveLocomotionClip() const
{
	switch ( m_locomotionState )
	{
	case EMonsterAnimState::Run:
		if ( !m_profile.runClip.empty() )
			return m_profile.runClip;
		if ( !m_profile.moveClip.empty() )
			return m_profile.moveClip;
		return m_profile.idleClip;

	case EMonsterAnimState::Move:
		if ( !m_profile.moveClip.empty() )
			return m_profile.moveClip;
		return m_profile.idleClip;

	case EMonsterAnimState::Idle:
	default:
		return m_profile.idleClip;
	}
}

bool CMonsterAnimController::CanAcceptHitReactionCommand() const
{
	if ( m_hitReactionAnimSuperArmorRemainingSec > 0.0f )
		return false;

	if ( !m_bHitReactionOnlyWhenNoAction )
		return true;

	// 보스용:
	// 현재 Attack / Spell / Hit / Appear / Call / Death 등 action 중이면
	// Hit 애니메이션을 받지 않는다.
	if ( m_actionPhase != EActionPhase::None )
		return false;

	// Spell / Attack 등이 pending으로 들어온 직후에도 Hit으로 덮지 않는다.
	if ( m_pendingCommand != EMonsterAnimCommand::None )
		return false;

	return true;
}

bool CMonsterAnimController::StartAction(CAnimator* anim, const std::string& clipName, EActionPhase phase, float blendTimeSec, bool loop)
{
	if ( !anim ) return false;
	if ( clipName.empty() ) return false;
	if ( !anim->HasClip(clipName) ) return false;

	if ( !anim->GetCurrentClipName().empty() )
	{
		if ( !anim->CrossFade(clipName, blendTimeSec, loop, 0.0f) )
			anim->Play(clipName, loop, 0.0f);
	}
	else
	{
		anim->Play(clipName, loop, 0.0f);
	}

	m_actionPhase = phase;
	return true;
}

void CMonsterAnimController::RequestCommand(EMonsterAnimCommand cmd)
{
	if ( cmd == EMonsterAnimCommand::None )
		return;

	// Death는 최우선
	if ( cmd == EMonsterAnimCommand::Death )
	{
		m_pendingCommand = cmd;
		return;
	}

	// Hit은 기본적으로는 기존처럼 높은 우선순위.
	// 단, 보스처럼 hit reaction policy가 설정된 경우에는
	// 현재 action 중이거나 애니메이션 슈퍼아머 시간 중이면 Hit 애니메이션만 무시한다.
	// 대미지/피격 사운드/피 튀김은 호출한 쪽에서 이미 처리하므로 여기서는 애니메이션만 필터링한다.
	if ( cmd == EMonsterAnimCommand::Hit )
	{
		if ( !CanAcceptHitReactionCommand() )
			return;

		m_pendingCommand = cmd;

		if ( m_hitReactionAnimSuperArmorDurationSec > 0.0f )
		{
			m_hitReactionAnimSuperArmorRemainingSec =
				m_hitReactionAnimSuperArmorDurationSec;
		}

		return;
	}

	// 그 외 명령은 현재 액션 중이면 무시
	if ( m_actionPhase != EActionPhase::None )
		return;

	m_pendingCommand = cmd;
}

void CMonsterAnimController::StartLocomotionIfNeeded(CAnimator* anim)
{
	if ( !anim ) return;
	if ( m_actionPhase != EActionPhase::None ) return;

	const std::string clip = ResolveLocomotionClip();
	if ( clip.empty() ) return;
	if ( !anim->HasClip(clip) ) return;

	if ( anim->GetCurrentClipName().empty() )
	{
		anim->Play(clip, true, 0.0f);
		return;
	}

	if ( anim->GetCurrentClipName() != clip )
	{
		if ( !anim->CrossFade(clip, m_profile.locomotionBlendTime, true, 0.0f) )
			anim->Play(clip, true, 0.0f);
	}
}

void CMonsterAnimController::Update(float dt)
{
	if ( dt > 0.0f && m_hitReactionAnimSuperArmorRemainingSec > 0.0f )
	{
		m_hitReactionAnimSuperArmorRemainingSec -= dt;

		if ( m_hitReactionAnimSuperArmorRemainingSec < 0.0f )
			m_hitReactionAnimSuperArmorRemainingSec = 0.0f;
	}

	CAnimator* anim = ResolveAnimator();
	if ( !anim ) return;

	if ( m_pendingCommand != EMonsterAnimCommand::None )
	{
		switch ( m_pendingCommand )
		{
		case EMonsterAnimCommand::Attack:
			StartAction(anim, m_profile.attackClip, EActionPhase::Attack, m_profile.actionBlendTime, false);
			break;

		case EMonsterAnimCommand::Hit:
			StartAction(anim, m_profile.hitClip, EActionPhase::Hit, m_profile.actionBlendTime, false);
			break;

		case EMonsterAnimCommand::Death:
			StartAction(anim, m_profile.deathClip, EActionPhase::Death, m_profile.actionBlendTime, false);
			break;

		case EMonsterAnimCommand::Appear:
			StartAction(anim, m_profile.appearClip, EActionPhase::Appear, 0.0f, false);
			break;

		case EMonsterAnimCommand::Call:
			StartAction(anim, m_profile.callClip, EActionPhase::Call, m_profile.actionBlendTime, false);
			break;

		case EMonsterAnimCommand::Spell:
			StartAction(anim, m_profile.spellClip, EActionPhase::Spell, m_profile.actionBlendTime, false);
			break;

		default:
			break;
		}

		m_pendingCommand = EMonsterAnimCommand::None;
	}

	if ( m_actionPhase != EActionPhase::None )
	{
		if ( !anim->IsCurrentClipFinished() )
			return;

		if ( ( m_actionPhase == EActionPhase::Attack ) &&
			m_profile.attackHasChain &&
			!m_profile.attackNextClip.empty() &&
			anim->HasClip(m_profile.attackNextClip) )
		{
			if ( !anim->CrossFade(m_profile.attackNextClip, m_profile.chainBlendTime, false, 0.0f) )
				anim->Play(m_profile.attackNextClip, false, 0.0f);

			m_actionPhase = EActionPhase::AttackChainNext;
			return;
		}

		if ( m_actionPhase == EActionPhase::Death )
			return;

		m_actionPhase = EActionPhase::None;
	}

	StartLocomotionIfNeeded(anim);
}