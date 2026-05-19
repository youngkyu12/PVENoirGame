//-----------------------------------------------------------------------------
// File: GhoulAIComponent.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GhoulAIComponent.h"

#include "GameScene.h"
#include "Object.h"
#include "AnimatorComponent.h"
#include "MonsterAnimController.h"
#include "HealthComponent.h"

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

bool CGhoulAIComponent::AcquireTarget()
{
	CGameScene* scene = GetScene();
	if ( !scene )
		return false;

	CGameObject* player = scene->GetPlayer();
	if ( !player )
		return false;

	if ( auto* hp = player->GetComponent<CHealthComponent>() )
	{
		if ( hp->IsDead() )
			return false;
	}

	SetTarget(player);
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