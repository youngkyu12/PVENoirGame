//-----------------------------------------------------------------------------
// File: GhoulAIComponent.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GhoulAIComponent.h"

#include "GameScene.h"
#include "Object.h"
#include "AnimatorComponent.h"
#include "MonsterAnimController.h"

CGhoulAIComponent::CGhoulAIComponent(CGameObject* owner)
	: CMonsterAIComponent(owner)
{
	// 추적 / 전투용 기본값
	SetMoveSpeed(2.0f);
	SetRepathInterval(0.15f);
	SetPathPointReachDistance(0.10f);
	SetGoalReachDistance(0.25f);

	SetDetectRange(99999.0f);
	SetAttackRange(1.5f);
	SetAttackCooldown(1.0f);
}

bool CGhoulAIComponent::AcquireTarget()
{
	CGameScene* scene = GetScene();
	if ( !scene )
		return false;

	// 테스트 조건: 무조건 player[0]만 추적
	CGameObject* player0 = scene->GetPlayerBySlot(0);
	if ( !player0 )
		return false;

	SetTarget(player0);
	return true;
}

void CGhoulAIComponent::UpdateBehavior(float dt)
{
	CMonsterAIComponent::UpdateBehavior(dt);
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