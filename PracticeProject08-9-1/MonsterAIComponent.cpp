//-----------------------------------------------------------------------------
// File: MonsterAIComponent.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "MonsterAIComponent.h"

#include <algorithm>
#include <cmath>

#include "GameScene.h"
#include "NavMesh.h"
#include "Grid.h"
#include "Object.h"
#include "AnimatorComponent.h"
#include "AnimController.h"
#include "PlayerControllerComponent.h"
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

	static bool IsDeadByHealth(const CGameObject* obj)
	{
		if ( !obj )
			return true;

		auto* hp = obj->GetComponent<CHealthComponent>();
		if ( !hp )
			return false;

		return hp->IsDead();
	}

	static bool IsNearlyZero(float v)
	{
		return ( std::fabs(v) <= 1e-6f );
	}

	static bool ComputeMegaGridCenterMovementBounds(
	const XMFLOAT3& pos,
	float& outMinX,
	float& outMaxX,
	float& outMinZ,
	float& outMaxZ)
	{
		if ( pos.x < static_cast< float >(CSceneGrid::kGridMinX) ||
			 pos.x > static_cast< float >( CSceneGrid::kGridMaxX ) )
		{
			return false;
		}

		if ( pos.z < static_cast< float >(CSceneGrid::kGridMinZ) ||
			 pos.z > static_cast< float >( CSceneGrid::kGridMaxZ ) )
		{
			return false;
		}

		int cellX = static_cast< int >( std::floor(pos.x) ) - CSceneGrid::kGridMinX;
		int cellZ = static_cast< int >( std::floor(pos.z) ) - CSceneGrid::kGridMinZ;

		if ( cellX == CSceneGrid::kGridWidth )
			cellX = CSceneGrid::kGridWidth - 1;

		if ( cellZ == CSceneGrid::kGridHeight )
			cellZ = CSceneGrid::kGridHeight - 1;

		if ( cellX < 0 || cellX >= CSceneGrid::kGridWidth )
			return false;

		if ( cellZ < 0 || cellZ >= CSceneGrid::kGridHeight )
			return false;

		const int megaX = cellX / CSceneGrid::kMegaGridCellWidth;
		const int megaZ = cellZ / CSceneGrid::kMegaGridCellHeight;

		if ( megaX < 0 || megaX >= CSceneGrid::kMegaGridCols )
			return false;

		if ( megaZ < 0 || megaZ >= CSceneGrid::kMegaGridRows )
			return false;

		const float megaMinX =
			static_cast< float >(CSceneGrid::kGridMinX + megaX * CSceneGrid::kMegaGridCellWidth);

		const float megaMinZ =
			static_cast< float >(CSceneGrid::kGridMinZ + megaZ * CSceneGrid::kMegaGridCellHeight);

		const float centerX =
			megaMinX + static_cast< float >(CSceneGrid::kMegaGridCellWidth) * 0.5f;

		const float centerZ =
			megaMinZ + static_cast< float >( CSceneGrid::kMegaGridCellHeight ) * 0.5f;

		constexpr float kMonsterMoveCenterSize = 200.0f;
		constexpr float kMonsterMoveCenterHalf = kMonsterMoveCenterSize * 0.5f;

		outMinX = centerX - kMonsterMoveCenterHalf;
		outMaxX = centerX + kMonsterMoveCenterHalf;
		outMinZ = centerZ - kMonsterMoveCenterHalf;
		outMaxZ = centerZ + kMonsterMoveCenterHalf;

		return true;
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

	if ( IsDeadByHealth(GetOwner()) )
	{
		ClearTarget();
		ClearPath();
		return;
	}

	if ( !m_pScene )
		return;

	EnsureMovementBoundsInitialized();

	UpdateCooldowns(dt);
	UpdatePathTimers(dt);

	if ( !CanThinkNow() )
	{
		ClearPath();
		return;
	}

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

bool CMonsterAIComponent::ForceChaseTarget(CGameObject* target)
{
	if ( !target )
		return false;

	if ( auto* hp = target->GetComponent<CHealthComponent>() )
	{
		if ( hp->IsDead() )
			return false;
	}

	SetTarget(target);
	ClearPath();
	m_repathTimer = 0.0f;
	return true;
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
	// 기존 이름 호환용.
	// 이제 detect range는 chase stop range와 같은 의미로 취급한다.
	return IsTargetInChaseStopRange();
}

bool CMonsterAIComponent::IsObjectInChaseStartRange(CGameObject* obj) const
{
	if ( !obj || !GetOwner() )
		return false;

	const float distSq =
		DistanceSqXZ(GetOwner()->GetPosition(), obj->GetPosition());

	return distSq <= ( m_chaseStartRange * m_chaseStartRange );
}

bool CMonsterAIComponent::IsObjectInChaseStopRange(CGameObject* obj) const
{
	if ( !obj || !GetOwner() )
		return false;

	const float distSq =
		DistanceSqXZ(GetOwner()->GetPosition(), obj->GetPosition());

	return distSq <= ( m_chaseStopRange * m_chaseStopRange );
}

bool CMonsterAIComponent::IsTargetInChaseStartRange() const
{
	if ( !HasValidTarget() )
		return false;

	return IsObjectInChaseStartRange(m_pTarget);
}

bool CMonsterAIComponent::IsTargetInChaseStopRange() const
{
	if ( !HasValidTarget() )
		return false;

	return IsObjectInChaseStopRange(m_pTarget);
}

bool CMonsterAIComponent::IsTargetInAttackRange() const
{
	if ( !HasValidTarget() || !GetOwner() )
		return false;

	const float distSq =
		DistanceSqXZ(GetOwner()->GetPosition(), m_pTarget->GetPosition());

	return distSq <= ( m_attackRange * m_attackRange );
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
	m_postAttackMoveLockRemaining = m_postAttackMoveLockDuration;
}

bool CMonsterAIComponent::RebuildPathToTarget()
{
	m_trianglePath.clear();
	m_currentPath.clear();
	m_currentPathIndex = 0;
	m_repathTimer = m_repathInterval;

	if ( !GetOwner() || !m_pTarget )
		return false;

	CNavMesh* nav = GetNavMesh();
	if ( !nav || !nav->IsLoaded() )
		return false;

	XMFLOAT3 startPos{};
	XMFLOAT3 goalPos{};

	if ( !SampleNavMeshPosition(GetOwner()->GetPosition(), startPos) )
		return false;

	const XMFLOAT3 moveGoalPos = GetTargetMoveGoalPosition();

	if ( !SampleNavMeshPosition(moveGoalPos, goalPos) )
		return false;

	goalPos = ClampPointToMovementBounds(goalPos);

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
	if ( !m_pScene )
		return false;

	CGameObject* player = m_pScene->GetPlayer();
	if ( !player )
		return false;

	if ( auto* hp = player->GetComponent<CHealthComponent>() )
	{
		if ( hp->IsDead() )
			return false;
	}

	if ( !ShouldAcquireTargetFromIdle(player) )
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

	if ( !IsTargetInChaseStopRange() )
	{
		ClearTarget();
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		return;
	}

	if ( IsTargetInAttackRange() && CanStartAttackAgainstTarget() )
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

	if ( TryMoveDirectlyToTarget(dt) )
	{
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
		FaceTowards(GetTargetMoveGoalPosition());
	}
}

bool CMonsterAIComponent::ShouldMoveTowardsTarget() const
{
	return true;
}

bool CMonsterAIComponent::CanStartAttackAgainstTarget() const
{
	return true;
}

bool CMonsterAIComponent::IsObjectInChaseStartCone(CGameObject* obj) const
{
	CGameObject* owner = GetOwner();
	if ( !owner || !obj )
		return false;

	const XMFLOAT3 ownerPos = owner->GetPosition();
	const XMFLOAT3 targetPos = obj->GetPosition();

	XMFLOAT3 toTarget(
		targetPos.x - ownerPos.x,
		0.0f,
		targetPos.z - ownerPos.z
	);

	const float toTargetLenSq =
		( toTarget.x * toTarget.x ) + ( toTarget.z * toTarget.z );

	if ( toTargetLenSq <= 1.0e-8f )
		return true;

	const float invTargetLen = 1.0f / std::sqrt(toTargetLenSq);
	toTarget.x *= invTargetLen;
	toTarget.z *= invTargetLen;

	const XMFLOAT4X4& world = owner->GetWorldMatrix();

	XMFLOAT3 forward(
		world._31,
		0.0f,
		world._33
	);

	const float forwardLenSq =
		( forward.x * forward.x ) + ( forward.z * forward.z );

	if ( forwardLenSq <= 1.0e-8f )
	{
		if ( auto* tr = owner->GetComponent<CTransformComponent>() )
		{
			// fallback: transform의 world matrix가 아직 갱신 전이어도
			// object world matrix 기준과 동일한 forward를 다시 시도한다.
			const XMFLOAT4X4& fallbackWorld = owner->GetWorldMatrix();
			forward = XMFLOAT3(fallbackWorld._31, 0.0f, fallbackWorld._33);
		}
	}

	const float fallbackForwardLenSq =
		( forward.x * forward.x ) + ( forward.z * forward.z );

	if ( fallbackForwardLenSq <= 1.0e-8f )
		return false;

	const float invForwardLen = 1.0f / std::sqrt(fallbackForwardLenSq);
	forward.x *= invForwardLen;
	forward.z *= invForwardLen;

	const float dot =
		( forward.x * toTarget.x ) + ( forward.z * toTarget.z );

	return dot >= m_chaseStartConeCosHalfAngle;
}

bool CMonsterAIComponent::IsPlayerRunning(CGameObject* player) const
{
	if ( !player )
		return false;

	if ( auto* controller = player->GetComponent<CPlayerControllerComponent>() )
	{
		if ( controller->IsRunLocomotionActive() )
			return true;
	}

	if ( auto* animComp = player->GetComponent<CAnimatorComponent>() )
	{
		if ( auto* ctrl = animComp->GetController() )
			return ctrl->IsRunLocomotionActive();
	}

	if ( auto* ctrl = player->GetAnimController() )
		return ctrl->IsRunLocomotionActive();

	return false;
}

bool CMonsterAIComponent::ShouldAcquireTargetFromIdle(CGameObject* candidate) const
{
	if ( !candidate )
		return false;

	if ( !IsObjectInChaseStartRange(candidate) )
		return false;

	if ( IsObjectInChaseStartCone(candidate) )
		return true;

	if ( IsPlayerRunning(candidate) )
		return true;

	return false;
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
	const XMFLOAT3 targetPos = GetTargetMoveGoalPosition();

	const float goalDeltaSq = DistanceSqXZ(lastPoint, targetPos);
	return goalDeltaSq > ( m_goalReachDistance * m_goalReachDistance );
}

bool CMonsterAIComponent::CanMoveNow() const
{
	if ( m_postAttackMoveLockRemaining > 0.0f )
		return false;

	if ( IsAIActionLockedByAnimation() )
		return false;

	return true;
}

bool CMonsterAIComponent::CanThinkNow() const
{
	if ( IsDeadByHealth(GetOwner()) )
		return false;

	if ( IsAIActionLockedByAnimation() )
		return false;

	return true;
}

bool CMonsterAIComponent::CanAttackNowByState() const
{
	if ( IsAIActionLockedByAnimation() )
		return false;

	return true;
}

bool CMonsterAIComponent::IsAIActionLockedByAnimation() const
{
	if ( auto* ctrl = GetMonsterAnimController() )
	{
		if ( ctrl->BlocksAIControl() )
			return true;
	}

	return false;
}

bool CMonsterAIComponent::EnsureMovementBoundsInitialized()
{
	if ( m_bMovementBoundsInitialized )
		return true;

	CGameObject* owner = GetOwner();
	if ( !owner )
		return false;

	float minX = 0.0f;
	float maxX = 0.0f;
	float minZ = 0.0f;
	float maxZ = 0.0f;

	if ( !ComputeMegaGridCenterMovementBounds(
		owner->GetPosition(),
		minX,
		maxX,
		minZ,
		maxZ) )
	{
		return false;
	}

	m_movementMinX = minX;
	m_movementMaxX = maxX;
	m_movementMinZ = minZ;
	m_movementMaxZ = maxZ;
	m_bMovementBoundsInitialized = true;

	return true;
}

XMFLOAT3 CMonsterAIComponent::ClampPointToMovementBounds(const XMFLOAT3& p) const
{
	if ( !m_bMovementBoundsInitialized )
		return p;

	XMFLOAT3 out = p;
	out.x = std::clamp(out.x, m_movementMinX, m_movementMaxX);
	out.z = std::clamp(out.z, m_movementMinZ, m_movementMaxZ);
	return out;
}

XMFLOAT3 CMonsterAIComponent::GetTargetMoveGoalPosition() const
{
	if ( !m_pTarget )
		return GetOwnerPosition();

	return ClampPointToMovementBounds(m_pTarget->GetPosition());
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

	if ( IsAIActionLockedByAnimation() )
		return false;

	const XMFLOAT3 faceTargetPos = ClampPointToMovementBounds(targetPos);

	XMFLOAT3 pos = owner->GetPosition();

	XMFLOAT3 dir(faceTargetPos.x - pos.x, 0.0f, faceTargetPos.z - pos.z);

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

	if ( !CanMoveNow() )
		return false;

	if ( maxStepDistance <= 0.0f )
		return false;

	const XMFLOAT3 moveTargetPos = ClampPointToMovementBounds(targetPos);

	XMFLOAT3 pos = owner->GetPosition();

	XMFLOAT3 delta(moveTargetPos.x - pos.x, 0.0f, moveTargetPos.z - pos.z);

	const float lenSq = ( delta.x * delta.x ) + ( delta.z * delta.z );
	if ( lenSq <= 1e-8f )
		return false;

	const float len = std::sqrt(lenSq);
	const float step = ( maxStepDistance < len ) ? maxStepDistance : len;
	const float invLen = 1.0f / len;

	XMFLOAT3 dir(delta.x * invLen, 0.0f, delta.z * invLen);

	FaceTowards(moveTargetPos);

	XMFLOAT3 newPos(
		pos.x + dir.x * step,
		pos.y,
		pos.z + dir.z * step
	);

	newPos = ClampPointToMovementBounds(newPos);

	// NavMesh 위로 다시 샘플링
	XMFLOAT3 sampled{};
	if ( SampleNavMeshPosition(newPos, sampled) )
	{
		owner->SetPosition(ClampPointToMovementBounds(sampled));
	}
	else
	{
		owner->SetPosition(newPos);
	}

	return true;
}

bool CMonsterAIComponent::TryMoveDirectlyToTarget(float dt)
{
	if ( !GetOwner() || !m_pTarget )
		return false;

	if ( dt <= 0.0f )
		return false;

	if ( !CanMoveNow() )
		return false;

	const float moveDistance = m_moveSpeed * dt;
	if ( moveDistance <= 0.0f )
		return false;

	const XMFLOAT3 goalPos = GetTargetMoveGoalPosition();

	if ( !HasDirectNavMeshLineTo(goalPos) )
		return false;

	m_trianglePath.clear();
	m_currentPath.clear();
	m_currentPathIndex = 0;

	SetMonsterLocomotionState(EMonsterAnimState::Run);
	return MoveTowards(goalPos, moveDistance);
}

bool CMonsterAIComponent::HasDirectNavMeshLineTo(
	const XMFLOAT3& targetPos,
	bool clampTargetToMovementBounds) const
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return false;

	CNavMesh* nav = GetNavMesh();
	if ( !nav || !nav->IsLoaded() )
		return false;

	XMFLOAT3 startPos{};
	XMFLOAT3 goalPos{};

	if ( !nav->SamplePosition(owner->GetPosition(), startPos, nullptr, 1.0f) )
		return false;

	const XMFLOAT3 desiredTargetPos =
		clampTargetToMovementBounds
		? ClampPointToMovementBounds(targetPos)
		: targetPos;

	if ( !nav->SamplePosition(desiredTargetPos, goalPos, nullptr, 1.0f) )
		return false;

	if ( clampTargetToMovementBounds )
		goalPos = ClampPointToMovementBounds(goalPos);

	return nav->HasLineOfSight(startPos, goalPos, 1.0f);
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
		const XMFLOAT3 waypoint =
			ClampPointToMovementBounds(m_currentPath[m_currentPathIndex]);

		const float dist = GetDistanceToPointXZ(waypoint);

		if ( dist <= m_pathPointReachDistance )
		{
			++m_currentPathIndex;
			continue;
		}

		const XMFLOAT3 ownerPos = GetOwner()->GetPosition();
		const XMFLOAT3 goalPos = GetTargetMoveGoalPosition();

		const float toWaypointX = waypoint.x - ownerPos.x;
		const float toWaypointZ = waypoint.z - ownerPos.z;
		const float toGoalX = goalPos.x - ownerPos.x;
		const float toGoalZ = goalPos.z - ownerPos.z;

		const float waypointLenSq =
			( toWaypointX * toWaypointX ) + ( toWaypointZ * toWaypointZ );

		const float goalLenSq =
			( toGoalX * toGoalX ) + ( toGoalZ * toGoalZ );

		if ( waypointLenSq > 1.0e-6f && goalLenSq > 1.0e-6f )
		{
			const float dot =
				( toWaypointX * toGoalX ) + ( toWaypointZ * toGoalZ );

			// 현재 waypoint가 목표 반대 방향에 있으면,
			// 기존 path가 현 상황에 안 맞는 것으로 보고 다음 frame에 재경로한다.
			if ( dot < 0.0f )
			{
				ClearPath();
				m_repathTimer = 0.0f;
				return false;
			}
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

	const bool actionLocked = IsAIActionLockedByAnimation();

	if ( !actionLocked && m_attackCooldownRemaining > 0.0f )
	{
		m_attackCooldownRemaining -= dt;
		if ( m_attackCooldownRemaining < 0.0f )
			m_attackCooldownRemaining = 0.0f;
	}

	if ( !actionLocked && m_postAttackMoveLockRemaining > 0.0f )
	{
		m_postAttackMoveLockRemaining -= dt;
		if ( m_postAttackMoveLockRemaining < 0.0f )
			m_postAttackMoveLockRemaining = 0.0f;
	}
}

void CMonsterAIComponent::UpdatePathTimers(float dt)
{
	if ( dt <= 0.0f )
		return;

	m_repathTimer -= dt;
}