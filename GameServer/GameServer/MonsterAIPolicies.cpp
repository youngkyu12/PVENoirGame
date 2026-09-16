#include "pch.h"
#include "MonsterAIPolicies.h"

#include "MonsterAI.h"
#include "Enemy.h"
#include "Room.h"
#include "MonsterAITrace.h"

bool CRangeConeTargetPolicy::FindTarget(CMonsterAI& ai) const
{
	return ai.FindRangeConeTarget(false);
}

bool CInnerZoneTargetPolicy::FindTarget(CMonsterAI& ai) const
{
	return ai.FindRangeConeTarget(true);
}

bool CBossRoomTargetPolicy::FindTarget(CMonsterAI& ai) const
{
	return ai.FindBossRoomTarget();
}

EMonsterMoveResult CNavMeshHybridChasePolicy::UpdateChase(CMonsterAI& ai, float dt) const
{
	if (!ai.CanMoveNow())
	{
		ai.SetIdleState();
		return EMonsterMoveResult::Blocked;
	}

	const auto targetPos = ai.m_pTarget->GetPosition();
	const auto moveGoalPos = ai.GetTargetMoveGoalPosition();
	ai.m_state = EMonsterAIState::Chase;

	if (ai.HasDirectNavMeshLineTo(moveGoalPos))
	{
		ai.ClearChasePath();
		return ai.MoveTowards(moveGoalPos, ai.m_moveSpeed * dt)
			? EMonsterMoveResult::Moving
			: EMonsterMoveResult::Reached;
	}

	if (ai.m_repathTimer <= 0.f || ai.m_currentPath.empty() || ai.m_currentPathIndex >= ai.m_currentPath.size())
		ai.RebuildPathToTarget();

	if (ai.FollowCurrentPath(dt))
		return EMonsterMoveResult::Moving;

	ai.FaceTowards(targetPos);
	ai.GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_IDLE);
	return EMonsterMoveResult::PathNotFound;
}

EMonsterMoveResult CDirectChasePolicy::UpdateChase(CMonsterAI& ai, float dt) const
{
	if (!ai.CanMoveNow())
	{
		ai.SetIdleState();
		return EMonsterMoveResult::Blocked;
	}

	ai.m_state = EMonsterAIState::Chase;
	return ai.MoveDirectTowards(ai.m_pTarget->GetPosition(), ai.m_moveSpeed * dt)
		? EMonsterMoveResult::Moving
		: EMonsterMoveResult::Reached;
}

bool CReturnIdleLifecyclePolicy::UpdateBeforeTargeting(CMonsterAI& ai, float dt) const
{
	if (!ai.m_bReturningHome)
		return false;

	ai.m_state = EMonsterAIState::ReturnHome;
	ai.UpdateReturnHome(dt);
	return true;
}

bool CReturnIdleLifecyclePolicy::UpdateNoTarget(CMonsterAI& ai, float, bool wasChasing) const
{
	(void)wasChasing;
	if (!ai.IsAtHome())
	{
		ai.m_state = EMonsterAIState::ReturnHome;
		ai.BeginReturnHome();
	}
	else
	{
		ai.SetIdleState();
	}

	return true;
}

bool CReturnPatrolLifecyclePolicy::UpdateBeforeTargeting(CMonsterAI& ai, float dt) const
{
	if (!ai.m_bReturningHome)
		return false;

	ai.m_state = EMonsterAIState::ReturnHome;
	ai.UpdateReturnHome(dt);
	return true;
}

bool CReturnPatrolLifecyclePolicy::UpdateNoTarget(CMonsterAI& ai, float dt, bool wasChasing) const
{
	if (wasChasing && !ai.IsAtHome())
	{
		ai.m_state = EMonsterAIState::ReturnHome;
		ai.BeginReturnHome();
	}
	else if (ai.UpdateIdlePatrol(dt))
	{
		ai.m_state = EMonsterAIState::Patrol;
	}
	else if (!ai.IsAtHome())
	{
		ai.m_state = EMonsterAIState::ReturnHome;
		ai.BeginReturnHome();
	}
	else
	{
		ai.SetIdleState();
	}

	return true;
}

bool CSpawnerRushLifecyclePolicy::UpdateBeforeTargeting(CMonsterAI& ai, float dt) const
{
	if (ai.m_initialAdvanceDist <= 0.f)
		return false;

	const float step = ai.m_moveSpeed * dt;
	const auto pos = ai.GetOwner()->GetPosition();
	const GameMath::Vec3 next(
		pos.x + ai.m_initialAdvanceDir.x * step,
		pos.y,
		pos.z + ai.m_initialAdvanceDir.z * step);

	ai.m_state = EMonsterAIState::InitialAdvance;
	ai.FaceTowards(next);
	ai.GetOwner()->SetPosition(next);
	ai.GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_RUN);
	ai.GetOwner()->SetLastMoveDir(ai.m_initialAdvanceDir);
	const bool completed = step >= ai.m_initialAdvanceDist;
	ai.m_initialAdvanceDist = std::max(0.f, ai.m_initialAdvanceDist - step);
	if (completed)
		MONSTER_AI_TRACE(ai, EMonsterAIState::InitialAdvance,
			EMonsterAITraceEvent::InitialAdvanceCompleted, true);
	return true;
}

bool CSpawnerRushLifecyclePolicy::UpdateNoTarget(CMonsterAI& ai, float, bool) const
{
	ai.SetIdleState();
	return true;
}

bool CBossRoomLifecyclePolicy::UpdateBeforeTargeting(CMonsterAI&, float) const
{
	return false;
}

bool CBossRoomLifecyclePolicy::UpdateNoTarget(CMonsterAI& ai, float, bool) const
{
	ai.SetIdleState();
	return true;
}

bool CBossRoomExitLifecyclePolicy::UpdateBeforeTargeting(CMonsterAI& ai, float dt) const
{
	if (!ai.m_bReturningHome)
		return false;

	ai.m_state = EMonsterAIState::ReturnHome;
	ai.UpdateReturnHome(dt);
	return true;
}

bool CBossRoomExitLifecyclePolicy::UpdateNoTarget(CMonsterAI& ai, float, bool wasChasing) const
{
	(void)wasChasing;
	if (!ai.IsAtHome())
	{
		ai.m_state = EMonsterAIState::ReturnHome;
		ai.BeginReturnHome();
	}
	else
	{
		ai.SetIdleState();
	}

	return true;
}
