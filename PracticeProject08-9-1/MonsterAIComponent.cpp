//-----------------------------------------------------------------------------
// File: MonsterAIComponent.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "MonsterAIComponent.h"

#include <algorithm>
#include <cmath>

#include "GameScene.h"
#include "NavMesh.h"
#include "Object.h"
#include "AnimatorComponent.h"
#include "AnimController.h"
#include "MonsterAnimController.h"
#include "ActorTagComponent.h"
#include "HealthComponent.h"

namespace
{
	static float DistanceSqXZ(const XMFLOAT3& a, const XMFLOAT3& b)
	{
		const float dx = a.x - b.x;
		const float dz = a.z - b.z;
		return ( dx * dx ) + ( dz * dz );
	}

	static float DistanceXZ(const XMFLOAT3& a, const XMFLOAT3& b)
	{
		return std::sqrt(DistanceSqXZ(a, b));
	}

	static bool IsNearlyZero(float v)
	{
		return ( std::fabs(v) <= 1e-6f );
	}
}

CMonsterAIComponent::CMonsterAIComponent(CGameObject* owner)
	: CComponentT<CMonsterAIComponent>(owner)
{
}

void CMonsterAIComponent::OnCreate(ID3D12Device* /*dev*/, ID3D12GraphicsCommandList* /*cmd*/)
{
	// scene은 GameScene에서 몬스터 생성 후 SetScene()으로 넣어주는 것을 기본 전제로 둔다.
	// 그래도 혹시 누락되어 있으면 여기서는 강제 추론하지 않는다.
}

void CMonsterAIComponent::OnUpdate(float dt)
{
	if ( !m_bAIEnabled )
		return;

	if ( !GetOwner() )
		return;

	if ( !m_pScene )
		return;

	if ( !CanThinkNow() )
		return;

	UpdateCooldowns(dt);
	UpdatePathTimers(dt);

	if ( !HasValidTarget() )
	{
		if ( !AcquireTarget() )
		{
			ClearTarget();
			ClearPath();
			return;
		}
	}

	if ( !HasValidTarget() )
	{
		ClearPath();
		return;
	}

	UpdateBehavior(dt);
}

void CMonsterAIComponent::SetTarget(CGameObject* target)
{
	m_pTarget = target;
	m_repathTimer = 0.0f;
}

void CMonsterAIComponent::ClearTarget()
{
	m_pTarget = nullptr;
	ClearPath();
}

bool CMonsterAIComponent::HasValidTarget() const
{
	if ( !m_pTarget )
		return false;

	if ( auto* hp = m_pTarget->GetComponent<CHealthComponent>() )
	{
		if ( hp->IsDead() )
			return false;
	}

	auto* tag = m_pTarget->GetComponent<CActorTagComponent>();
	if ( !tag )
		return true;

	return true;
}

float CMonsterAIComponent::GetDistanceToTargetXZ() const
{
	if ( !GetOwner() || !m_pTarget )
		return FLT_MAX;

	return DistanceXZ(GetOwner()->GetPosition(), m_pTarget->GetPosition());
}

float CMonsterAIComponent::GetDistanceToPointXZ(const XMFLOAT3& p) const
{
	if ( !GetOwner() )
		return FLT_MAX;

	return DistanceXZ(GetOwner()->GetPosition(), p);
}

bool CMonsterAIComponent::IsTargetInDetectRange() const
{
	if ( !HasValidTarget() )
		return false;

	return ( GetDistanceToTargetXZ() <= m_detectRange );
}

bool CMonsterAIComponent::IsTargetInAttackRange() const
{
	if ( !HasValidTarget() )
		return false;

	return ( GetDistanceToTargetXZ() <= m_attackRange );
}

bool CMonsterAIComponent::CanAttackNow() const
{
	if ( m_attackCooldownRemaining > 0.0f )
		return false;

	if ( !CanAttackNowByState() )
		return false;

	return true;
}

void CMonsterAIComponent::ConsumeAttackCooldown()
{
	m_attackCooldownRemaining = m_attackCooldown;
}

bool CMonsterAIComponent::RebuildPathToTarget()
{
	m_trianglePath.clear();
	m_currentPath.clear();
	m_currentPathIndex = 0;
	m_repathTimer = 0.0f;

	if ( !GetOwner() || !m_pTarget )
		return false;

	CNavMesh* nav = GetNavMesh();
	if ( !nav || !nav->IsLoaded() )
		return false;

	XMFLOAT3 startPos{};
	XMFLOAT3 goalPos{};

	if ( !SampleNavMeshPosition(GetOwner()->GetPosition(), startPos) )
		return false;

	if ( !SampleNavMeshPosition(m_pTarget->GetPosition(), goalPos) )
		return false;

	if ( !nav->FindPath(startPos, goalPos, m_trianglePath, m_currentPath) )
		return false;

	m_currentPathIndex = 0;

	// 시작점과 거의 같은 첫 포인트는 바로 건너뛴다.
	while ( m_currentPathIndex < m_currentPath.size() )
	{
		if ( DistanceXZ(GetOwner()->GetPosition(), m_currentPath[m_currentPathIndex]) > m_pathPointReachDistance )
			break;

		++m_currentPathIndex;
	}

	return HasPath();
}

void CMonsterAIComponent::ClearPath()
{
	m_trianglePath.clear();
	m_currentPath.clear();
	m_currentPathIndex = 0;
	SetMonsterLocomotionState(EMonsterAnimState::Idle);
}

bool CMonsterAIComponent::AcquireTarget()
{
	// 기본 정책:
	// 싱글플레이 현재 단계에서는 local player를 타겟으로 삼는다.
	if ( !m_pScene )
		return false;

	CGameObject* player = m_pScene->GetPlayer();
	if ( !player )
		return false;

	SetTarget(player);
	return true;
}

void CMonsterAIComponent::UpdateBehavior(float dt)
{
	if ( !HasValidTarget() )
	{
		ClearPath();
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		return;
	}

	// 감지 범위 밖이면 추적 중단
	if ( !IsTargetInDetectRange() )
	{
		ClearPath();
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		return;
	}

	// 공격 범위면 이동 정지 + 공격
	if ( IsTargetInAttackRange() )
	{
		ClearPath();
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		FaceTowards(m_pTarget->GetPosition());

		if ( CanAttackNow() )
		{
			if ( TryPerformAttack() )
			{
				ConsumeAttackCooldown();
			}
		}
		return;
	}

	// 이동 가능하면 경로 추적
	if ( !ShouldMoveTowardsTarget() )
	{
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		return;
	}

	if ( !CanMoveNow() )
	{
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		return;
	}

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
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		FaceTowards(m_pTarget->GetPosition());
	}
}

bool CMonsterAIComponent::ShouldMoveTowardsTarget() const
{
	return true;
}

bool CMonsterAIComponent::ShouldRepath() const
{
	if ( !HasValidTarget() )
		return false;

	if ( m_repathTimer <= 0.0f )
		return true;

	if ( !HasPath() )
		return true;

	// 목표가 마지막 경로점과 많이 달라졌으면 재경로
	if ( m_currentPath.empty() )
		return true;

	const XMFLOAT3& lastPoint = m_currentPath.back();
	const XMFLOAT3 targetPos = m_pTarget->GetPosition();

	const float goalDelta = DistanceXZ(lastPoint, targetPos);
	return ( goalDelta > m_goalReachDistance );
}

bool CMonsterAIComponent::CanMoveNow() const
{
	if ( auto* ctrl = GetMonsterAnimController() )
	{
		if ( ctrl->IsBusy() )
			return false;
	}

	return true;
}

bool CMonsterAIComponent::CanThinkNow() const
{
	// 추후 death state 체크 가능
	return true;
}

bool CMonsterAIComponent::CanAttackNowByState() const
{
	if ( auto* ctrl = GetMonsterAnimController() )
	{
		if ( ctrl->IsBusy() )
			return false;
	}

	return true;
}

CNavMesh* CMonsterAIComponent::GetNavMesh() const
{
	if ( !m_pScene )
		return nullptr;

	return m_pScene->GetNavMesh();
}

CAnimatorComponent* CMonsterAIComponent::GetAnimatorComponent() const
{
	if ( !GetOwner() )
		return nullptr;

	return GetOwner()->GetComponent<CAnimatorComponent>();
}

CAnimController* CMonsterAIComponent::GetAnimController() const
{
	if ( !GetOwner() )
		return nullptr;

	if ( auto* animComp = GetOwner()->GetComponent<CAnimatorComponent>() )
	{
		if ( auto* ctrl = animComp->GetController() )
			return ctrl;
	}

	return GetOwner()->GetAnimController();
}

CMonsterAnimController* CMonsterAIComponent::GetMonsterAnimController() const
{
	if ( !GetOwner() )
		return nullptr;

	if ( auto* animComp = GetOwner()->GetComponent<CAnimatorComponent>() )
	{
		return animComp->EnsureMonsterController();
	}

	return nullptr;
}

void CMonsterAIComponent::SetMonsterLocomotionState(EMonsterAnimState state)
{
	if ( auto* ctrl = GetMonsterAnimController() )
	{
		ctrl->SetLocomotionState(state);
	}
}

XMFLOAT3 CMonsterAIComponent::GetOwnerPosition() const
{
	if ( !GetOwner() )
		return XMFLOAT3(0.0f, 0.0f, 0.0f);

	return GetOwner()->GetPosition();
}

bool CMonsterAIComponent::FaceTowards(const XMFLOAT3& targetPos)
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return false;

	XMFLOAT3 pos = owner->GetPosition();

	XMFLOAT3 dir(
		targetPos.x - pos.x,
		0.0f,
		targetPos.z - pos.z
	);

	const float lenSq = ( dir.x * dir.x ) + ( dir.z * dir.z );
	if ( lenSq <= 1e-8f )
		return false;

	const float invLen = 1.0f / std::sqrt(lenSq);
	dir.x *= invLen;
	dir.z *= invLen;

	if ( auto* tr = owner->GetComponent<CTransformComponent>() )
	{
		tr->SetLookDirection(dir, XMFLOAT3(0.0f, 1.0f, 0.0f));

		if ( std::fabs(m_facingYawOffsetDegrees) > 1e-6f )
		{
			tr->RotateWorldEulerDegrees(0.0f, m_facingYawOffsetDegrees, 0.0f);
		}

		return true;
	}

	return false;
}

bool CMonsterAIComponent::MoveTowards(const XMFLOAT3& targetPos, float maxStepDistance)
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return false;

	if ( maxStepDistance <= 0.0f )
		return false;

	XMFLOAT3 pos = owner->GetPosition();

	XMFLOAT3 delta(
		targetPos.x - pos.x,
		0.0f,
		targetPos.z - pos.z
	);

	const float lenSq = ( delta.x * delta.x ) + ( delta.z * delta.z );
	if ( lenSq <= 1e-8f )
		return false;

	const float len = std::sqrt(lenSq);
	const float step = ( maxStepDistance < len ) ? maxStepDistance : len;
	const float invLen = 1.0f / len;

	XMFLOAT3 dir(
		delta.x * invLen,
		0.0f,
		delta.z * invLen
	);

	FaceTowards(targetPos);

	XMFLOAT3 newPos(
		pos.x + dir.x * step,
		pos.y,
		pos.z + dir.z * step
	);

	// NavMesh 위로 다시 샘플링
	XMFLOAT3 sampled{};
	if ( SampleNavMeshPosition(newPos, sampled) )
	{
		owner->SetPosition(sampled);
	}
	else
	{
		owner->SetPosition(newPos);
	}

	return true;
}

bool CMonsterAIComponent::FollowCurrentPath(float dt)
{
	if ( !GetOwner() )
		return false;

	if ( !HasPath() )
	{
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		return false;
	}

	if ( dt <= 0.0f )
		return false;

	const float moveDistance = m_moveSpeed * dt;
	if ( moveDistance <= 0.0f )
		return false;

	while ( m_currentPathIndex < m_currentPath.size() )
	{
		const XMFLOAT3& waypoint = m_currentPath[m_currentPathIndex];
		const float dist = GetDistanceToPointXZ(waypoint);

		if ( dist <= m_pathPointReachDistance )
		{
			++m_currentPathIndex;
			continue;
		}

		SetMonsterLocomotionState(EMonsterAnimState::Run);
		MoveTowards(waypoint, moveDistance);
		return true;
	}

	SetMonsterLocomotionState(EMonsterAnimState::Idle);
	return false;
}

bool CMonsterAIComponent::SampleNavMeshPosition(const XMFLOAT3& inPos, XMFLOAT3& outPos, int* outTri) const
{
	CNavMesh* nav = GetNavMesh();
	if ( !nav || !nav->IsLoaded() )
		return false;

	return nav->SamplePosition(inPos, outPos, outTri);
}

void CMonsterAIComponent::UpdateCooldowns(float dt)
{
	if ( dt <= 0.0f )
		return;

	if ( m_attackCooldownRemaining > 0.0f )
	{
		m_attackCooldownRemaining -= dt;
		if ( m_attackCooldownRemaining < 0.0f )
			m_attackCooldownRemaining = 0.0f;
	}
}

void CMonsterAIComponent::UpdatePathTimers(float dt)
{
	if ( dt <= 0.0f )
		return;

	m_repathTimer -= dt;
}