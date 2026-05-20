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
#include "ColliderComponent.h"

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

	static float QuaternionToYawDegreesLocal(const XMFLOAT4& q)
	{
		const float siny_cosp = 2.0f * ( q.w * q.y + q.x * q.z );
		const float cosy_cosp = 1.0f - 2.0f * ( q.y * q.y + q.z * q.z );

		return XMConvertToDegrees(std::atan2(siny_cosp, cosy_cosp));
	}

	static float NormalizeYawDegrees180(float yawDeg)
	{
		while ( yawDeg > 180.0f )
			yawDeg -= 360.0f;

		while ( yawDeg < -180.0f )
			yawDeg += 360.0f;

		return yawDeg;
	}

	static float NormalizeYawDegrees360(float yawDeg)
	{
		while ( yawDeg >= 360.0f )
			yawDeg -= 360.0f;

		while ( yawDeg < 0.0f )
			yawDeg += 360.0f;

		return yawDeg;
	}

	static XMFLOAT3 ForwardFromYawDegrees(float yawDeg)
	{
		const float yawRad = XMConvertToRadians(yawDeg);

		return XMFLOAT3(
			std::sin(yawRad),
			0.0f,
			std::cos(yawRad)
		);
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

	EnsureHomeTransformCaptured();

	if ( IsDeadByHealth(GetOwner()) )
	{
		ClearTarget();
		ClearPath();
		ClearReturnHomePath();
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

	if ( m_pTarget && !HasValidTarget() )
	{
		BeginReturnHome();
	}

	if ( !HasValidTarget() )
	{
		if ( m_bReturningHome )
		{
			UpdateReturnHome(dt);
			return;
		}

		if ( !AcquireTarget() )
		{
			// SwordMan / BowMan은 target이 없고 복귀 중도 아니면 순찰한다.
			if ( UpdateIdlePatrol(dt) )
				return;

			ClearPath();
			SetMonsterLocomotionState(EMonsterAnimState::Idle);
			return;
		}
	}

	if ( !HasValidTarget() )
	{
		if ( m_bReturningHome )
		{
			UpdateReturnHome(dt);
		}
		else if ( !UpdateIdlePatrol(dt) )
		{
			SetMonsterLocomotionState(EMonsterAnimState::Idle);
		}

		return;
	}

	UpdateBehavior(dt);
}

void CMonsterAIComponent::SetTarget(CGameObject* target)
{
	m_pTarget = target;
	m_repathTimer = 0.0f;

	if ( target )
	{
		ClearReturnHomePath();
		ResetPatrolState();
	}
}

bool CMonsterAIComponent::ForceChaseTarget(CGameObject* target)
{
	if ( !target )
		return false;

	if ( m_pScene && !m_pScene->IsLocalMonsterChaseEnabled() )
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

void CMonsterAIComponent::StopChaseAndReturnHome()
{
	CGameObject* owner = GetOwner();

	if ( !owner )
		return;

	if ( IsDeadByHealth(owner) )
	{
		ClearTarget();
		ClearPath();
		ClearReturnHomePath();
		return;
	}

	EnsureHomeTransformCaptured();

	if ( IsAtHome() )
	{
		ClearTarget();
		ClearPath();
		ClearReturnHomePath();
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		return;
	}

	BeginReturnHome();
}

void CMonsterAIComponent::SetHomeTransform(const XMFLOAT3& position, float yawDeg)
{
	m_homePosition = position;
	m_homeYawDeg = yawDeg;
	m_bHomeTransformCaptured = true;

	ResetPatrolState();
}

void CMonsterAIComponent::CaptureHomeTransformFromOwner()
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return;

	const XMFLOAT3 pos = owner->GetPosition();

	float yawDeg = 0.0f;

	if ( auto* tr = owner->GetComponent<CTransformComponent>() )
	{
		yawDeg = QuaternionToYawDegreesLocal(tr->rotation);
	}
	else
	{
		const XMFLOAT4X4& world = owner->GetWorldMatrix();

		const float fx = world._31;
		const float fz = world._33;

		if ( ( fx * fx + fz * fz ) > 1.0e-8f )
			yawDeg = XMConvertToDegrees(std::atan2(fx, fz));
	}

	SetHomeTransform(pos, yawDeg);
}

bool CMonsterAIComponent::EnsureHomeTransformCaptured()
{
	if ( m_bHomeTransformCaptured )
		return true;

	CaptureHomeTransformFromOwner();
	return m_bHomeTransformCaptured;
}

void CMonsterAIComponent::ResetToHomeTransformForMegaGridSkip()
{
	if ( !EnsureHomeTransformCaptured() )
		return;

	CGameObject* owner = GetOwner();
	if ( !owner )
		return;

	m_pTarget = nullptr;

	ClearPath();
	ClearReturnHomePath();
	ResetPatrolState();

	owner->SetPosition(m_homePosition);

	if ( auto* tr = owner->GetComponent<CTransformComponent>() )
	{
		tr->SetYawDegrees(m_homeYawDeg);
	}

	if ( auto* collider = owner->GetComponent<CColliderComponent>() )
	{
		collider->UpdateWorldBounds();
	}

	SetMonsterLocomotionState(EMonsterAnimState::Idle);
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

void CMonsterAIComponent::ClearReturnHomePath()
{
	m_bReturningHome = false;
	m_returnTrianglePath.clear();
	m_returnPath.clear();
	m_returnPathIndex = 0;
}

bool CMonsterAIComponent::IsAtHome(float tolerance) const
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return false;

	const float distSq =
		DistanceSqXZ(owner->GetPosition(), m_homePosition);

	return distSq <= ( tolerance * tolerance );
}

bool CMonsterAIComponent::BeginReturnHome()
{
	if ( !EnsureHomeTransformCaptured() )
		return false;

	m_pTarget = nullptr;
	ClearPath();
	ClearReturnHomePath();
	ResetPatrolState();

	if ( IsAtHome() )
	{
		ResetToHomeTransformForMegaGridSkip();
		return true;
	}

	return BuildReturnPathToHomeOnce();
}

bool CMonsterAIComponent::BuildReturnPathToHomeOnce()
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return false;

	CNavMesh* nav = GetNavMesh();
	if ( !nav || !nav->IsLoaded() )
		return false;

	m_returnTrianglePath.clear();
	m_returnPath.clear();
	m_returnPathIndex = 0;

	XMFLOAT3 startPos{};
	XMFLOAT3 goalPos{};

	if ( !SampleNavMeshPosition(owner->GetPosition(), startPos) )
		return false;

	if ( !SampleNavMeshPosition(m_homePosition, goalPos) )
		return false;

	if ( !nav->FindPath(startPos, goalPos, m_returnTrianglePath, m_returnPath) )
		return false;

	while ( m_returnPathIndex < m_returnPath.size() )
	{
		if ( DistanceXZ(owner->GetPosition(), m_returnPath[m_returnPathIndex]) >
			 m_pathPointReachDistance )
		{
			break;
		}

		++m_returnPathIndex;
	}

	m_bReturningHome =
		!m_returnPath.empty() &&
		( m_returnPathIndex < m_returnPath.size() );

	if ( !m_bReturningHome && IsAtHome() )
	{
		ResetToHomeTransformForMegaGridSkip();
		return true;
	}

	return m_bReturningHome;
}

bool CMonsterAIComponent::AcquireTarget()
{
	if ( !m_pScene )
		return false;

	CGameObject* player = m_pScene->GetPlayer();
	if ( !player )
		return false;

	if ( m_pScene && !m_pScene->IsLocalMonsterChaseEnabled() )
		return false;

	if ( auto* hp = player->GetComponent<CHealthComponent>() )
	{
		if ( hp->IsDead() )
			return false;
	}

	if ( !CanChaseTargetByMegaGridCenter(player) )
		return false;

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

	if ( m_pScene && !m_pScene->IsLocalMonsterChaseEnabled() )
	{
		BeginReturnHome();
		return;
	}

	if ( !CanChaseTargetByMegaGridCenter(m_pTarget) )
	{
		BeginReturnHome();
		return;
	}

	if ( !IsTargetInChaseStopRange() )
	{
		BeginReturnHome();
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

bool CMonsterAIComponent::CanChaseTargetByMegaGridCenter(CGameObject* target) const
{
	if ( !target )
		return false;

	if ( !m_pScene )
		return true;

	if ( !m_pScene->IsLocalPlayer(target) )
		return true;

	return m_pScene->IsLocalPlayerInsideMegaGridCenter();
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

float CMonsterAIComponent::GetChaseMoveSpeed() const
{
	return m_runMoveSpeed;
}

float CMonsterAIComponent::GetWalkMoveSpeed() const
{
	return m_walkMoveSpeed;
}

EMonsterAnimState CMonsterAIComponent::GetChaseLocomotionState() const
{
	return m_bChaseUsesRunAnimation
		? EMonsterAnimState::Run
		: EMonsterAnimState::Move;
}

EMonsterAnimState CMonsterAIComponent::GetWalkLocomotionState() const
{
	return EMonsterAnimState::Move;
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

bool CMonsterAIComponent::FaceTowardsNoClamp(const XMFLOAT3& targetPos)
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return false;

	if ( IsAIActionLockedByAnimation() )
		return false;

	const XMFLOAT3 pos = owner->GetPosition();

	XMFLOAT3 dir(
		targetPos.x - pos.x,
		0.0f,
		targetPos.z - pos.z
	);

	const float lenSq = ( dir.x * dir.x ) + ( dir.z * dir.z );
	if ( lenSq <= 1.0e-8f )
		return false;

	const float invLen = 1.0f / std::sqrt(lenSq);
	dir.x *= invLen;
	dir.z *= invLen;

	if ( auto* tr = owner->GetComponent<CTransformComponent>() )
	{
		tr->SetLookDirection(dir, XMFLOAT3(0.0f, 1.0f, 0.0f));

		if ( std::fabs(m_facingYawOffsetDegrees) > 1.0e-6f )
		{
			tr->RotateWorldEulerDegrees(
				0.0f,
				m_facingYawOffsetDegrees,
				0.0f
			);
		}

		return true;
	}

	return false;
}

bool CMonsterAIComponent::MoveTowardsNoClamp(
	const XMFLOAT3& targetPos,
	float maxStepDistance)
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return false;

	if ( !CanMoveNow() )
		return false;

	if ( maxStepDistance <= 0.0f )
		return false;

	const XMFLOAT3 pos = owner->GetPosition();

	XMFLOAT3 delta(
		targetPos.x - pos.x,
		0.0f,
		targetPos.z - pos.z
	);

	const float lenSq = ( delta.x * delta.x ) + ( delta.z * delta.z );
	if ( lenSq <= 1.0e-8f )
		return false;

	const float len = std::sqrt(lenSq);
	const float step = ( maxStepDistance < len ) ? maxStepDistance : len;
	const float invLen = 1.0f / len;

	XMFLOAT3 dir(
		delta.x * invLen,
		0.0f,
		delta.z * invLen
	);

	FaceTowardsNoClamp(targetPos);

	XMFLOAT3 newPos(
		pos.x + dir.x * step,
		pos.y,
		pos.z + dir.z * step
	);

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

bool CMonsterAIComponent::TryMoveDirectlyToTarget(float dt)
{
	if ( !GetOwner() || !m_pTarget )
		return false;

	if ( dt <= 0.0f )
		return false;

	if ( !CanMoveNow() )
		return false;

	const float moveDistance = GetChaseMoveSpeed() * dt;
	if ( moveDistance <= 0.0f )
		return false;

	const XMFLOAT3 goalPos = GetTargetMoveGoalPosition();

	if ( !HasDirectNavMeshLineTo(goalPos) )
		return false;

	m_trianglePath.clear();
	m_currentPath.clear();
	m_currentPathIndex = 0;

	SetMonsterLocomotionState(GetChaseLocomotionState());
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

	const float moveDistance = GetChaseMoveSpeed() * dt;
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

		SetMonsterLocomotionState(GetChaseLocomotionState());
		MoveTowards(waypoint, moveDistance);
		return true;
	}

	SetMonsterLocomotionState(EMonsterAnimState::Idle);
	return false;
}

bool CMonsterAIComponent::UpdateReturnHome(float dt)
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return false;

	if ( !m_bReturningHome )
		return false;

	if ( IsAtHome() )
	{
		ResetToHomeTransformForMegaGridSkip();
		return true;
	}

	if ( dt <= 0.0f )
		return false;

	if ( !CanMoveNow() )
	{
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		return true;
	}

	const float moveDistance = GetWalkMoveSpeed() * dt;
	if ( moveDistance <= 0.0f )
		return false;

	while ( m_returnPathIndex < m_returnPath.size() )
	{
		const XMFLOAT3 waypoint = m_returnPath[m_returnPathIndex];

		const float dist = GetDistanceToPointXZ(waypoint);

		if ( dist <= m_pathPointReachDistance )
		{
			++m_returnPathIndex;
			continue;
		}

		SetMonsterLocomotionState(GetWalkLocomotionState());
		MoveTowardsNoClamp(waypoint, moveDistance);
		return true;
	}

	ResetToHomeTransformForMegaGridSkip();
	return true;
}

void CMonsterAIComponent::SetPatrolEnabled(bool enabled)
{
	if ( m_bPatrolEnabled == enabled )
		return;

	m_bPatrolEnabled = enabled;

	if ( !enabled )
		ResetPatrolState();
}

void CMonsterAIComponent::ResetPatrolState()
{
	m_bPatrolInitialized = false;
	m_bPatrolTurning = false;
	m_patrolTargetSign = 1;
	m_patrolTurnTargetYawDeg = 0.0f;
}

bool CMonsterAIComponent::EnsurePatrolInitialized()
{
	if ( !m_bPatrolEnabled )
		return false;

	if ( m_bPatrolInitialized )
		return true;

	if ( !EnsureHomeTransformCaptured() )
		return false;

	m_patrolTargetSign = 1;
	m_bPatrolTurning = false;
	m_patrolTurnTargetYawDeg =
		GetPatrolFacingYawDegreesForTargetSign(m_patrolTargetSign);

	m_bPatrolInitialized = true;
	return true;
}

XMFLOAT3 CMonsterAIComponent::GetPatrolEndpoint(int targetSign) const
{
	const int sign = ( targetSign < 0 ) ? -1 : 1;

	const XMFLOAT3 forward = ForwardFromYawDegrees(m_homeYawDeg);

	return XMFLOAT3(
		m_homePosition.x + forward.x * m_patrolHalfDistance * static_cast< float >(sign),
		m_homePosition.y,
		m_homePosition.z + forward.z * m_patrolHalfDistance * static_cast< float >(sign)
	);
}

float CMonsterAIComponent::GetPatrolFacingYawDegreesForTargetSign(int targetSign) const
{
	if ( targetSign < 0 )
		return NormalizeYawDegrees360(m_homeYawDeg + 180.0f);

	return NormalizeYawDegrees360(m_homeYawDeg);
}

float CMonsterAIComponent::GetOwnerYawDegrees() const
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return 0.0f;

	if ( auto* tr = owner->GetComponent<CTransformComponent>() )
		return QuaternionToYawDegreesLocal(tr->rotation);

	const XMFLOAT4X4& world = owner->GetWorldMatrix();

	const float fx = world._31;
	const float fz = world._33;

	if ( ( fx * fx + fz * fz ) <= 1.0e-8f )
		return 0.0f;

	return XMConvertToDegrees(std::atan2(fx, fz));
}

void CMonsterAIComponent::SetOwnerYawDegrees(float yawDeg)
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return;

	yawDeg = NormalizeYawDegrees360(yawDeg);

	if ( auto* tr = owner->GetComponent<CTransformComponent>() )
	{
		tr->SetYawDegrees(yawDeg);
		return;
	}

	const float currentYaw = GetOwnerYawDegrees();
	const float deltaYaw = NormalizeYawDegrees180(yawDeg - currentYaw);

	owner->Rotate(0.0f, deltaYaw, 0.0f);
}

bool CMonsterAIComponent::RotateOwnerYawTowards(float targetYawDeg, float maxStepDeg)
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return false;

	if ( maxStepDeg <= 0.0f )
		return false;

	targetYawDeg = NormalizeYawDegrees360(targetYawDeg);

	const float currentYaw = GetOwnerYawDegrees();
	const float deltaYaw = NormalizeYawDegrees180(targetYawDeg - currentYaw);

	const float absDelta = std::fabs(deltaYaw);

	if ( absDelta <= 0.5f )
	{
		SetOwnerYawDegrees(targetYawDeg);
		return true;
	}

	const float step =
		std::clamp(deltaYaw, -maxStepDeg, maxStepDeg);

	SetOwnerYawDegrees(currentYaw + step);

	return absDelta <= maxStepDeg;
}

bool CMonsterAIComponent::UpdateIdlePatrol(float dt)
{
	if ( !m_bPatrolEnabled )
		return false;

	if ( HasValidTarget() )
		return false;

	if ( m_bReturningHome )
		return false;

	CGameObject* owner = GetOwner();
	if ( !owner )
		return false;

	if ( IsDeadByHealth(owner) )
		return false;

	if ( !EnsurePatrolInitialized() )
		return false;

	if ( dt <= 0.0f )
	{
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		return true;
	}

	if ( !CanMoveNow() )
	{
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		return true;
	}

	if ( m_bPatrolTurning )
	{
		SetMonsterLocomotionState(EMonsterAnimState::Move);

		const bool finishedTurn =
			RotateOwnerYawTowards(
				m_patrolTurnTargetYawDeg,
				m_patrolTurnSpeedDegrees * dt
			);

		if ( finishedTurn )
			m_bPatrolTurning = false;

		return true;
	}

	const XMFLOAT3 endpoint = GetPatrolEndpoint(m_patrolTargetSign);
	const float distToEndpoint = DistanceXZ(owner->GetPosition(), endpoint);

	if ( distToEndpoint <= m_patrolEndpointReachDistance )
	{
		m_patrolTargetSign = -m_patrolTargetSign;
		m_patrolTurnTargetYawDeg =
			GetPatrolFacingYawDegreesForTargetSign(m_patrolTargetSign);
		m_bPatrolTurning = true;

		SetMonsterLocomotionState(EMonsterAnimState::Move);

		RotateOwnerYawTowards(
			m_patrolTurnTargetYawDeg,
			m_patrolTurnSpeedDegrees * dt
		);

		return true;
	}

	const float moveDistance = GetWalkMoveSpeed() * dt;
	if ( moveDistance <= 0.0f )
		return true;

	SetMonsterLocomotionState(GetWalkLocomotionState());
	MoveTowardsNoClamp(endpoint, moveDistance);
	return true;
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