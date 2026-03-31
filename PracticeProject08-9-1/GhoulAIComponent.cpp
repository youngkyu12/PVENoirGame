//-----------------------------------------------------------------------------
// File: GhoulAIComponent.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GhoulAIComponent.h"

#include "GameScene.h"
#include "Object.h"

CGhoulAIComponent::CGhoulAIComponent(CGameObject* owner)
	: CMonsterAIComponent(owner)
{
	// NavMesh 추적 테스트용 기본값
	SetMoveSpeed(2.0f);
	SetRepathInterval(0.15f);
	SetPathPointReachDistance(0.10f);
	SetGoalReachDistance(0.25f);

	// Ghoul 메쉬 정면축 보정
	SetFacingYawOffsetDegrees(180.0f);

	// 테스트용이므로 감지/공격 사거리는 의미 없지만,
	// 베이스 쪽 값이 남아 있어도 문제 없게 넉넉히 둔다.
	SetDetectRange(99999.0f);
	SetAttackRange(0.0f);
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
	if ( !HasValidTarget() )
	{
		ClearPath();
		return;
	}

	// 목표를 향해 무조건 NavMesh 경로 재계산/추적
	if ( ShouldRepath() || !HasPath() )
	{
		RebuildPathToTarget();
	}

	if ( HasPath() )
	{
		FollowCurrentPath(dt);
	}
	else
	{
		// 혹시 path가 안 나오더라도 최소한 바라보게는 둔다.
		FaceTowards(GetTarget()->GetPosition());
	}
}

bool CGhoulAIComponent::TryPerformAttack()
{
	// 이번 테스트 목적에서는 공격 안 함
	return false;
}