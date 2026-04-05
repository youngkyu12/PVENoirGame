//-----------------------------------------------------------------------------
// File: MonsterCombatComponent.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "MonsterCombatComponent.h"

#include "Object.h"
#include "AnimatorComponent.h"
#include "MonsterAnimController.h"
#include "MonsterAnimTypes.h"

CMonsterCombatComponent::CMonsterCombatComponent(CGameObject* owner)
	: CComponentT<CMonsterCombatComponent>(owner)
{
}

void CMonsterCombatComponent::OnUpdate(float dt)
{
	if ( !IsEnabled() )
		return;

	if ( m_hitCooldownRemaining > 0.0f )
	{
		m_hitCooldownRemaining -= dt;
		if ( m_hitCooldownRemaining < 0.0f )
			m_hitCooldownRemaining = 0.0f;
	}
}

bool CMonsterCombatComponent::CanTakeHit() const
{
	if ( !IsEnabled() ) return false;
	if ( m_bDead ) return false;
	if ( m_hitCooldownRemaining > 0.0f ) return false;
	return true;
}

void CMonsterCombatComponent::OnHitByPlayerWeapon(CGameObject* weaponObject, bool forceDeath)
{
	UNREFERENCED_PARAMETER(weaponObject);

	if ( !IsEnabled() )
		return;

	if ( m_bDead )
		return;

	if ( forceDeath )
	{
		m_bDead = true;
		RequestDeathAnimation();
		return;
	}

	if ( !CanTakeHit() )
		return;

	m_hitCooldownRemaining = m_hitCooldownSec;
	RequestHitAnimation();
}

void CMonsterCombatComponent::ForceDeath()
{
	if ( m_bDead )
		return;

	m_bDead = true;
	RequestDeathAnimation();
}

void CMonsterCombatComponent::RequestHitAnimation()
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return;

	auto* animComp = owner->GetComponent<CAnimatorComponent>();
	if ( !animComp )
		return;

	auto* monsterCtrl = animComp->EnsureMonsterController();
	if ( !monsterCtrl )
		return;

	monsterCtrl->RequestCommand(EMonsterAnimCommand::Hit);
}

void CMonsterCombatComponent::RequestDeathAnimation()
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return;

	auto* animComp = owner->GetComponent<CAnimatorComponent>();
	if ( !animComp )
		return;

	auto* monsterCtrl = animComp->EnsureMonsterController();
	if ( !monsterCtrl )
		return;

	monsterCtrl->RequestCommand(EMonsterAnimCommand::Death);
}