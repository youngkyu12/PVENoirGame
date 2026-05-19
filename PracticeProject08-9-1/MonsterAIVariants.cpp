//-----------------------------------------------------------------------------
// File: MonsterAIVariants.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "MonsterAIVariants.h"

#include "Object.h"
#include "AnimatorComponent.h"
#include "MonsterAnimController.h"

//-----------------------------------------------------------------------------
// Ghoul
//-----------------------------------------------------------------------------
CGhoulAIComponent::CGhoulAIComponent(CGameObject* owner)
	: CMonsterAIComponent(owner)
{
	SetMoveSpeed(2.0f);
	SetRepathInterval(0.35f);
	SetPathPointReachDistance(0.20f);
	SetGoalReachDistance(0.85f);

	SetDetectRange(50.0f);
	SetAttackRange(1.5f);
	SetAttackCooldown(1.0f);
}

bool CGhoulAIComponent::TryPerformAttack()
{
	auto* animComp = GetAnimatorComponent();
	if ( !animComp )
		return false;

	auto* ctrl = animComp->EnsureMonsterController();
	if ( !ctrl )
		return false;

	ctrl->RequestCommand(EMonsterAnimCommand::Attack);
	return true;
}

//-----------------------------------------------------------------------------
// SwordMan
//-----------------------------------------------------------------------------
CSwordManAIComponent::CSwordManAIComponent(CGameObject* owner)
	: CMonsterAIComponent(owner)
{
	SetMoveSpeed(2.0f);
	SetRepathInterval(0.35f);
	SetPathPointReachDistance(0.20f);
	SetGoalReachDistance(0.85f);

	SetDetectRange(50.0f);

	// Ghoul 공격 거리 1.5 기준.
	// SwordMan은 Ghoul 키의 1.5배이고 롱소드를 들기 때문에
	// 맨손 키 보정치 1.5 * 1.5 = 2.25보다 조금 더 길게 둔다.
	SetAttackRange(3.0f);

	SetAttackCooldown(1.0f);
}

bool CSwordManAIComponent::TryPerformAttack()
{
	auto* animComp = GetAnimatorComponent();
	if ( !animComp )
		return false;

	auto* ctrl = animComp->EnsureMonsterController();
	if ( !ctrl )
		return false;

	ctrl->RequestCommand(EMonsterAnimCommand::Attack);
	return true;
}

//-----------------------------------------------------------------------------
// BowMan
//-----------------------------------------------------------------------------
CBowManAIComponent::CBowManAIComponent(CGameObject* owner)
	: CMonsterAIComponent(owner)
{
	SetMoveSpeed(2.0f);
	SetRepathInterval(0.35f);
	SetPathPointReachDistance(0.20f);
	SetGoalReachDistance(0.85f);

	SetDetectRange(50.0f);

	// BowMan은 장거리 공격 몬스터.
	// 25m 안에 들어오면 정지 후 정면으로 활을 발사한다.
	SetAttackRange(25.0f);

	SetAttackCooldown(1.0f);
}

bool CBowManAIComponent::TryPerformAttack()
{
	auto* animComp = GetAnimatorComponent();
	if ( !animComp )
		return false;

	auto* ctrl = animComp->EnsureMonsterController();
	if ( !ctrl )
		return false;

	ctrl->RequestCommand(EMonsterAnimCommand::Attack);
	return true;
}

//-----------------------------------------------------------------------------
// Mutant
//-----------------------------------------------------------------------------
CMutantAIComponent::CMutantAIComponent(CGameObject* owner)
	: CMonsterAIComponent(owner)
{
	SetMoveSpeed(2.0f);
	SetRepathInterval(0.35f);
	SetPathPointReachDistance(0.20f);
	SetGoalReachDistance(0.85f);

	SetDetectRange(50.0f);

	// Ghoul 맨손 공격 거리 1.5 기준.
	// Mutant는 Ghoul 키의 1.8배이고 맨손이므로 1.5 * 1.8.
	SetAttackRange(2.7f);

	SetAttackCooldown(1.0f);
}

bool CMutantAIComponent::TryPerformAttack()
{
	auto* animComp = GetAnimatorComponent();
	if ( !animComp )
		return false;

	auto* ctrl = animComp->EnsureMonsterController();
	if ( !ctrl )
		return false;

	ctrl->RequestCommand(EMonsterAnimCommand::Attack);
	return true;
}

//-----------------------------------------------------------------------------
// Boss
//-----------------------------------------------------------------------------
CBossAIComponent::CBossAIComponent(CGameObject* owner)
	: CMonsterAIComponent(owner)
{
	SetMoveSpeed(2.0f);
	SetRepathInterval(0.35f);
	SetPathPointReachDistance(0.20f);
	SetGoalReachDistance(0.85f);

	SetDetectRange(50.0f);

	// Ghoul 맨손 공격 거리 1.5 기준.
	// Boss는 Ghoul 키의 2.5배이고 현재는 맨손 공격만 사용.
	SetAttackRange(3.75f);

	SetAttackCooldown(1.0f);
}

bool CBossAIComponent::TryPerformAttack()
{
	auto* animComp = GetAnimatorComponent();
	if ( !animComp )
		return false;

	auto* ctrl = animComp->EnsureMonsterController();
	if ( !ctrl )
		return false;

	ctrl->RequestCommand(EMonsterAnimCommand::Attack);
	return true;
}