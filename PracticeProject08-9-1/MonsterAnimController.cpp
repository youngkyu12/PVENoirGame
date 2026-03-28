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
	if ( m_locomotionState == EMonsterAnimState::Move )
		return m_profile.moveClip;

	return m_profile.idleClip;
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

	if ( cmd == EMonsterAnimCommand::Death )
	{
		m_pendingCommand = cmd;
		return;
	}

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
	( void ) dt;

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
			StartAction(anim, m_profile.appearClip, EActionPhase::Appear, m_profile.actionBlendTime, false);
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