#include "stdafx.h"
#include "MonsterAIVariants.h"

#include "GameScene.h"
#include "Grid.h"
#include "Object.h"
#include "AnimatorComponent.h"
#include "MonsterAnimController.h"
#include "HealthComponent.h"
#include "ColliderComponent.h"

namespace
{
	static bool IsWorldPositionInsideMegaGridCenterZone(
		const XMFLOAT3& pos,
		int megaGridNumber)
	{
		if ( megaGridNumber < 1 || megaGridNumber > CSceneGrid::kMegaGridCount )
			return false;

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

		const int expectedZeroBased = megaGridNumber - 1;
		const int expectedMegaX = expectedZeroBased % CSceneGrid::kMegaGridCols;
		const int expectedMegaZ = expectedZeroBased / CSceneGrid::kMegaGridCols;

		const int actualMegaX = cellX / CSceneGrid::kMegaGridCellWidth;
		const int actualMegaZ = cellZ / CSceneGrid::kMegaGridCellHeight;

		if ( actualMegaX != expectedMegaX || actualMegaZ != expectedMegaZ )
			return false;

		const int zoneWidth =
			std::clamp(
				CSceneGrid::kDefaultMegaGridApproachWidth,
				1,
				CSceneGrid::kMegaGridCellWidth
			);

		const int zoneHeight =
			std::clamp(
				CSceneGrid::kDefaultMegaGridApproachHeight,
				1,
				CSceneGrid::kMegaGridCellHeight
			);

		const int megaStartX = expectedMegaX * CSceneGrid::kMegaGridCellWidth;
		const int megaStartZ = expectedMegaZ * CSceneGrid::kMegaGridCellHeight;

		const int zoneStartX =
			megaStartX + ( ( CSceneGrid::kMegaGridCellWidth - zoneWidth ) / 2 );

		const int zoneStartZ =
			megaStartZ + ( ( CSceneGrid::kMegaGridCellHeight - zoneHeight ) / 2 );

		const int zoneEndX = zoneStartX + zoneWidth;
		const int zoneEndZ = zoneStartZ + zoneHeight;

		return
			( cellX >= zoneStartX && cellX < zoneEndX ) &&
			( cellZ >= zoneStartZ && cellZ < zoneEndZ );
	}

	static float ComputeYawDegreesToPointXZ(
	const XMFLOAT3& from,
	const XMFLOAT3& to)
	{
		const float dx = to.x - from.x;
		const float dz = to.z - from.z;

		if ( ( dx * dx + dz * dz ) <= 1.0e-8f )
			return 0.0f;

		return XMConvertToDegrees(std::atan2(dx, dz));
	}

	static float DistanceSqXZLocal(const XMFLOAT3& a, const XMFLOAT3& b)
	{
		const float dx = a.x - b.x;
		const float dz = a.z - b.z;
		return dx * dx + dz * dz;
	}

	static XMFLOAT3 ForwardFromYawDegreesLocal(float yawDeg)
	{
		const float yawRad = XMConvertToRadians(yawDeg);

		return XMFLOAT3(
			std::sin(yawRad),
			0.0f,
			std::cos(yawRad)
		);
	}

	static bool IsObjectDeadByHealthLocal(const CGameObject* obj)
	{
		if ( !obj )
			return true;

		auto* hp = obj->GetComponent<CHealthComponent>();
		if ( !hp )
			return false;

		return hp->IsDead();
	}

	static float SmoothStep01Local(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return t * t * ( 3.0f - 2.0f * t );
	}
}

//-----------------------------------------------------------------------------
// Ghoul
//-----------------------------------------------------------------------------
CGhoulAIComponent::CGhoulAIComponent(CGameObject* owner)
	: CMonsterAIComponent(owner)
{
	SetMoveSpeeds(1.0f, 2.0f);
	SetRepathInterval(0.35f);
	SetPathPointReachDistance(0.20f);
	SetGoalReachDistance(0.85f);

	SetChaseRanges(10.0f, 50.0f);
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
// Enemy Spawner Ghoul
//-----------------------------------------------------------------------------
CEnemySpawnerGhoulAIComponent::CEnemySpawnerGhoulAIComponent(CGameObject* owner)
	: CGhoulAIComponent(owner)
{
	const float runSpeed = GetRunMoveSpeedValue();

	// 이 AI는 배회/복귀/walk 이동이 없다.
	// 혹시 walk speed가 호출되더라도 run speed와 같게 둔다.
	SetMoveSpeeds(runSpeed, runSpeed);
	SetChaseRunAnimationEnabled(true);
	SetPatrolEnabled(false);

	m_bInitialAdvanceActive = true;
	m_initialAdvanceRemainingDistance = 60.0f;
}

void CEnemySpawnerGhoulAIComponent::ConfigureSpawnerGhoulAI(
	int megaGridNumber,
	float initialAdvanceDistance)
{
	m_spawnerMegaGridNumber = megaGridNumber;

	m_initialAdvanceRemainingDistance =
		( initialAdvanceDistance > 0.0f ) ? initialAdvanceDistance : 0.0f;

	m_bInitialAdvanceActive =
		( m_initialAdvanceRemainingDistance > 0.0f );
}

void CEnemySpawnerGhoulAIComponent::OnUpdate(float dt)
{
	if ( !m_bAIEnabled )
		return;

	CGameObject* owner = GetOwner();
	if ( !owner )
		return;

	EnsureHomeTransformCaptured();

	if ( IsObjectDeadByHealthLocal(owner) )
	{
		ClearTarget();
		ClearPath();
		ClearReturnHomePath();
		return;
	}

	if ( !m_pScene )
		return;

	UpdateCooldowns(dt);
	UpdatePathTimers(dt);

	if ( !CanThinkNow() )
	{
		ClearPath();
		return;
	}

	// 갓 생성된 상태에서는 타겟 여부와 무관하게 먼저 60m 직진.
	if ( m_bInitialAdvanceActive )
	{
		UpdateInitialAdvance(dt);
		return;
	}

	// 이후에는 타겟이 없거나, 타겟이 100x100 밖으로 나가면 즉시 정지.
	if ( !HasValidTarget() || !IsPlayerValidSpawnerTarget(m_pTarget) )
	{
		ClearTarget();
		AcquireTarget();
	}

	UpdateBehavior(dt);
}

bool CEnemySpawnerGhoulAIComponent::ForceChaseTarget(CGameObject* target)
{
	if ( !target )
		return false;

	if ( m_pScene && !m_pScene->IsLocalMonsterChaseEnabled() )
		return false;

	if ( !IsPlayerValidSpawnerTarget(target) )
		return false;

	SetTarget(target);
	ClearPath();
	m_repathTimer = 0.0f;
	return true;
}

bool CEnemySpawnerGhoulAIComponent::AcquireTarget()
{
	if ( !m_pScene )
		return false;

	if ( !m_pScene->IsLocalMonsterChaseEnabled() )
		return false;

	// 60m 강제 직진 중에는 타겟을 새로 잡지 않는다.
	if ( m_bInitialAdvanceActive )
		return false;

	CGameObject* nearest = FindNearestPlayerInsideInnerEmptyZone();
	if ( !nearest )
		return false;

	SetTarget(nearest);
	return true;
}

void CBossAIComponent::ScheduleBossCallMonsterSpawn()
{
	m_bBossCallMonsterSpawnPending = true;
	m_bossCallMonsterSpawnPendingCallIndex = m_bossExecutedCallCount;
	m_bossCallMonsterSpawnElapsedSec = 0.0f;
	m_bossCallMonsterSpawnDelaySec = 0.0f;

	if ( CGameScene* scene = GetScene() )
	{
		m_bossCallMonsterSpawnDelaySec =
			scene->GetBossCallMonsterSpawnDelaySec();
	}

	if ( m_bossCallMonsterSpawnDelaySec < 0.0f )
		m_bossCallMonsterSpawnDelaySec = 0.0f;

	if ( m_bossCallMonsterSpawnDelaySec <= 1.0e-6f )
	{
		ExecuteBossCallMonsterSpawn("immediate");
	}
}

bool CBossAIComponent::UpdateBossCallMonsterSpawnDelay(float dt)
{
	if ( !m_bBossCallMonsterSpawnPending )
		return false;

	if ( dt > 0.0f )
		m_bossCallMonsterSpawnElapsedSec += dt;

	if ( m_bossCallMonsterSpawnElapsedSec + 1.0e-6f <
		 m_bossCallMonsterSpawnDelaySec )
	{
		return true;
	}

	ExecuteBossCallMonsterSpawn("delay_elapsed");
	return true;
}

void CBossAIComponent::ExecuteBossCallMonsterSpawn(const char* reason)
{
	if ( !m_bBossCallMonsterSpawnPending )
		return;

	const int callIndex = m_bossCallMonsterSpawnPendingCallIndex;

	m_bBossCallMonsterSpawnPending = false;
	m_bossCallMonsterSpawnPendingCallIndex = -1;

	if ( CGameScene* scene = GetScene() )
	{
		scene->SpawnBossCallMonsters(callIndex);
	}

	m_bossCallMonsterSpawnDelaySec = 0.0f;
	m_bossCallMonsterSpawnElapsedSec = 0.0f;
}

bool CBossAIComponent::UpdateBossCallSequence(
	float dt,
	CGameObject* target,
	bool allowStartPendingCall)
{
	if ( !m_bBossSummonEnabled )
		return false;

	if ( UpdateBossCallRise(dt) )
		return true;

	if ( m_bBossCallDescendPendingOnCallEnd && !IsBossCallActionPlaying() )
	{
		if ( m_bBossCallMonsterSpawnPending )
		{
			ExecuteBossCallMonsterSpawn("call_end_flush");
		}

		BeginBossCallDescend();
	}

	if ( UpdateBossCallDescend(dt) )
		return true;

	if ( IsBossCallActionPlaying() )
	{
		ClearPath();

		// Call phase에 실제로 진입한 순간에 pending을 소비한다.
		// 단, 실제 몬스터 생성은 즉시 하지 않고 delay 타이머를 예약한다.
		if ( m_bBossCallConsumePendingOnStart )
		{
			if ( m_bossPendingCallCount > 0 )
				--m_bossPendingCallCount;

			m_bBossCallConsumePendingOnStart = false;

			++m_bossExecutedCallCount;

			ScheduleBossCallMonsterSpawn();

			ConsumeBossCallCooldown();

			m_bBossCallDescendPendingOnCallEnd = true;
			m_bBossCallRiseCompletedForCurrentCall = false;
		}

		// Call 애니메이션 시작 후 delay가 지난 시점에 실제 소환.
		UpdateBossCallMonsterSpawnDelay(dt);

		m_bBossCallCommandRequested = false;
		m_bossCallRequestAgeSec = 0.0f;

		return true;
	}

	if ( allowStartPendingCall && HasPendingBossCall() )
	{
		TryRequestPendingBossCall(target, dt);
		return true;
	}

	return false;
}

void CEnemySpawnerGhoulAIComponent::UpdateBehavior(float dt)
{
	if ( m_pScene && !m_pScene->IsLocalMonsterChaseEnabled() )
	{
		ClearTarget();
		ClearPath();
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		return;
	}

	if ( !HasValidTarget() || !IsPlayerValidSpawnerTarget(m_pTarget) )
	{
		ClearTarget();
		ClearPath();
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		return;
	}

	if ( IsTargetInAttackRange() && CanStartAttackAgainstTarget() )
	{
		ClearPath();
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		FaceTowardsNoClamp(m_pTarget->GetPosition());

		if ( CanAttackNow() )
		{
			if ( TryPerformAttack() )
				ConsumeAttackCooldown();
		}

		return;
	}

	if ( !CanMoveNow() )
	{
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		return;
	}

	if ( dt <= 0.0f )
	{
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		return;
	}

	const float moveDistance = GetChaseMoveSpeed() * dt;
	if ( moveDistance <= 0.0f )
	{
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		return;
	}

	SetMonsterLocomotionState(EMonsterAnimState::Run);
	MoveDirectNoNavTowards(m_pTarget->GetPosition(), moveDistance);
}

EMonsterAnimState CEnemySpawnerGhoulAIComponent::GetChaseLocomotionState() const
{
	return EMonsterAnimState::Run;
}

EMonsterAnimState CEnemySpawnerGhoulAIComponent::GetWalkLocomotionState() const
{
	return EMonsterAnimState::Run;
}

bool CEnemySpawnerGhoulAIComponent::UpdateInitialAdvance(float dt)
{
	if ( !m_bInitialAdvanceActive )
		return false;

	if ( m_initialAdvanceRemainingDistance <= 0.0f )
	{
		m_bInitialAdvanceActive = false;
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		return false;
	}

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

	float moveDistance = GetChaseMoveSpeed() * dt;

	if ( moveDistance > m_initialAdvanceRemainingDistance )
		moveDistance = m_initialAdvanceRemainingDistance;

	if ( moveDistance <= 0.0f )
	{
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		return true;
	}

	const XMFLOAT3 forward = ForwardFromYawDegreesLocal(m_homeYawDeg);

	SetMonsterLocomotionState(EMonsterAnimState::Run);

	if ( MoveDirectNoNavByDirection(forward, moveDistance) )
	{
		m_initialAdvanceRemainingDistance -= moveDistance;

		if ( m_initialAdvanceRemainingDistance <= 0.0f )
		{
			m_initialAdvanceRemainingDistance = 0.0f;
			m_bInitialAdvanceActive = false;
			ClearTarget();
			ClearPath();
			SetMonsterLocomotionState(EMonsterAnimState::Idle);
		}
	}

	return true;
}

bool CEnemySpawnerGhoulAIComponent::IsPlayerValidSpawnerTarget(CGameObject* player) const
{
	if ( !player )
		return false;

	if ( !m_pScene )
		return false;

	bool isRegisteredPlayer = false;

	for ( int slot = 0; slot < 4; ++slot )
	{
		if ( m_pScene->GetPlayerBySlot(slot) == player )
		{
			isRegisteredPlayer = true;
			break;
		}
	}

	if ( !isRegisteredPlayer )
		return false;

	if ( auto* hp = player->GetComponent<CHealthComponent>() )
	{
		if ( hp->IsDead() )
			return false;
	}

	return IsWorldPositionInsideInnerEmptyZone(player->GetPosition());
}

bool CEnemySpawnerGhoulAIComponent::GetInnerEmptyZoneCenter(XMFLOAT3& outCenter) const
{
	if ( m_spawnerMegaGridNumber < 1 ||
		 m_spawnerMegaGridNumber > CSceneGrid::kMegaGridCount )
	{
		return false;
	}

	const int zeroBased = m_spawnerMegaGridNumber - 1;
	const int megaX = zeroBased % CSceneGrid::kMegaGridCols;
	const int megaZ = zeroBased / CSceneGrid::kMegaGridCols;

	outCenter.x =
		static_cast< float >(
			CSceneGrid::kGridMinX +
			megaX * CSceneGrid::kMegaGridCellWidth +
			CSceneGrid::kMegaGridCellWidth / 2
		);

	outCenter.y = 0.0f;

	outCenter.z =
		static_cast< float >(
			CSceneGrid::kGridMinZ +
			megaZ * CSceneGrid::kMegaGridCellHeight +
			CSceneGrid::kMegaGridCellHeight / 2
		);

	return true;
}

bool CEnemySpawnerGhoulAIComponent::IsWorldPositionInsideInnerEmptyZone(
	const XMFLOAT3& pos) const
{
	XMFLOAT3 center{};
	if ( !GetInnerEmptyZoneCenter(center) )
		return false;

	const float minX = center.x - kInnerEmptyZoneHalfExtent;
	const float maxX = center.x + kInnerEmptyZoneHalfExtent;
	const float minZ = center.z - kInnerEmptyZoneHalfExtent;
	const float maxZ = center.z + kInnerEmptyZoneHalfExtent;

	// 요구사항: 경계선에 있으면 무시.
	// 따라서 <=, >=가 아니라 strict comparison을 쓴다.
	return
		( pos.x > minX && pos.x < maxX ) &&
		( pos.z > minZ && pos.z < maxZ );
}

CGameObject* CEnemySpawnerGhoulAIComponent::FindNearestPlayerInsideInnerEmptyZone() const
{
	if ( !m_pScene )
		return nullptr;

	CGameObject* owner = GetOwner();
	if ( !owner )
		return nullptr;

	CGameObject* bestPlayer = nullptr;
	float bestDistSq = FLT_MAX;

	const XMFLOAT3 ownerPos = owner->GetPosition();

	for ( int slot = 0; slot < 4; ++slot )
	{
		CGameObject* player = m_pScene->GetPlayerBySlot(slot);
		if ( !IsPlayerValidSpawnerTarget(player) )
			continue;

		const float distSq =
			DistanceSqXZLocal(ownerPos, player->GetPosition());

		if ( distSq < bestDistSq )
		{
			bestDistSq = distSq;
			bestPlayer = player;
		}
	}

	return bestPlayer;
}

bool CEnemySpawnerGhoulAIComponent::MoveDirectNoNavTowards(
	const XMFLOAT3& targetPos,
	float maxStepDistance)
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return false;

	if ( maxStepDistance <= 0.0f )
		return false;

	const XMFLOAT3 pos = owner->GetPosition();

	XMFLOAT3 delta(
		targetPos.x - pos.x,
		0.0f,
		targetPos.z - pos.z
	);

	const float lenSq = delta.x * delta.x + delta.z * delta.z;
	if ( lenSq <= 1.0e-8f )
		return false;

	const float len = std::sqrt(lenSq);
	const float invLen = 1.0f / len;

	XMFLOAT3 dir(
		delta.x * invLen,
		0.0f,
		delta.z * invLen
	);

	const float stepDistance =
		( maxStepDistance < len ) ? maxStepDistance : len;

	return MoveDirectNoNavByDirection(
		dir,
		stepDistance
	);
}

bool CEnemySpawnerGhoulAIComponent::MoveDirectNoNavByDirection(
	const XMFLOAT3& direction,
	float maxStepDistance)
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return false;

	if ( maxStepDistance <= 0.0f )
		return false;

	XMFLOAT3 dir(direction.x, 0.0f, direction.z);

	const float lenSq = dir.x * dir.x + dir.z * dir.z;
	if ( lenSq <= 1.0e-8f )
		return false;

	const float invLen = 1.0f / std::sqrt(lenSq);
	dir.x *= invLen;
	dir.z *= invLen;

	const XMFLOAT3 pos = owner->GetPosition();

	const XMFLOAT3 lookTarget(
		pos.x + dir.x,
		pos.y,
		pos.z + dir.z
	);

	FaceTowardsNoClamp(lookTarget);

	XMFLOAT3 newPos(
		pos.x + dir.x * maxStepDistance,
		pos.y,
		pos.z + dir.z * maxStepDistance
	);

	owner->SetPosition(newPos);

	if ( auto* collider = owner->GetComponent<CColliderComponent>() )
		collider->UpdateWorldBounds();

	return true;
}

//-----------------------------------------------------------------------------
// Boss Stage Monster
//-----------------------------------------------------------------------------
CBossStageMonsterAIComponent::CBossStageMonsterAIComponent(CGameObject* owner)
	: CMonsterAIComponent(owner)
{
	ConfigureBossStageMonsterAI(EKind::Ghoul);
}

void CBossStageMonsterAIComponent::ConfigureBossStageMonsterAI(EKind kind)
{
	m_kind = kind;

	SetRepathInterval(0.35f);
	SetPathPointReachDistance(0.20f);
	SetGoalReachDistance(0.85f);

	// 추적 시작/종료 거리는 5번 보스 스테이지 AI에서는 쓰지 않는다.
	// 실제 추적 여부는 플레이어가 5번 보스 스테이지에 있는지만 본다.
	SetChaseRanges(1000000.0f, 1000000.0f);

	SetAttackCooldown(1.0f);
	SetChaseRunAnimationEnabled(true);

	// SwordMan / BowMan의 기존 전후 patrol 이동 제거.
	SetPatrolEnabled(false);

	switch ( m_kind )
	{
	case EKind::Ghoul:
		SetMoveSpeeds(1.0f, 2.0f);
		SetAttackRange(1.5f);
		break;

	case EKind::SwordMan:
		SetMoveSpeeds(4.0f, 8.0f);
		SetAttackRange(3.0f);
		break;

	case EKind::BowMan:
		SetMoveSpeeds(4.0f, 8.0f);
		SetAttackRange(25.0f);
		break;

	case EKind::Mutant:
		SetMoveSpeeds(5.0f, 12.0f);
		SetAttackRange(2.7f);
		break;

	default:
		SetMoveSpeeds(1.0f, 2.0f);
		SetAttackRange(1.5f);
		break;
	}
}

void CBossStageMonsterAIComponent::OnUpdate(float dt)
{
	if ( !m_bAIEnabled )
		return;

	CGameObject* owner = GetOwner();
	if ( !owner )
		return;

	EnsureHomeTransformCaptured();

	if ( IsObjectDeadByHealthLocal(owner) )
	{
		ClearTarget();
		ClearPath();
		ClearReturnHomePath();
		return;
	}

	if ( !m_pScene )
		return;

	UpdateCooldowns(dt);
	UpdatePathTimers(dt);

	if ( !CanThinkNow() )
	{
		ClearPath();
		return;
	}

	const bool hasBossStagePlayer = HasAnyValidPlayerInsideBossStage();

	if ( !hasBossStagePlayer )
	{
		ClearTarget();

		if ( !IsAtHome() )
		{
			if ( !m_bReturningHome )
				BeginReturnHome();

			if ( m_bReturningHome )
			{
				UpdateReturnHome(dt);
				return;
			}
		}

		ClearPath();
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		return;
	}

	if ( !HasValidTarget() || !IsPlayerValidBossStageTarget(m_pTarget) )
	{
		ClearTarget();
		AcquireTarget();
	}

	if ( !HasValidTarget() )
	{
		if ( !IsAtHome() )
		{
			if ( !m_bReturningHome )
				BeginReturnHome();

			if ( m_bReturningHome )
			{
				UpdateReturnHome(dt);
				return;
			}
		}

		ClearPath();
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		return;
	}

	UpdateBehavior(dt);
}

bool CBossStageMonsterAIComponent::ForceChaseTarget(CGameObject* target)
{
	if ( !target )
		return false;

	if ( m_pScene && !m_pScene->IsLocalMonsterChaseEnabled() )
		return false;

	if ( !IsPlayerValidBossStageTarget(target) )
		return false;

	SetTarget(target);
	ClearPath();
	m_repathTimer = 0.0f;
	return true;
}

bool CBossStageMonsterAIComponent::AcquireTarget()
{
	if ( !m_pScene )
		return false;

	if ( !m_pScene->IsLocalMonsterChaseEnabled() )
		return false;

	CGameObject* nearest = FindNearestPlayerInsideBossStage();
	if ( !nearest )
		return false;

	SetTarget(nearest);
	return true;
}

void CBossStageMonsterAIComponent::UpdateBehavior(float dt)
{
	if ( m_pScene && !m_pScene->IsLocalMonsterChaseEnabled() )
	{
		BeginReturnHome();
		return;
	}

	if ( !HasValidTarget() || !IsPlayerValidBossStageTarget(m_pTarget) )
	{
		BeginReturnHome();
		return;
	}

	if ( IsTargetInAttackRange() && CanStartAttackAgainstTarget() )
	{
		ClearPath();
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		FaceTowardsNoClamp(m_pTarget->GetPosition());

		if ( CanAttackNow() )
		{
			if ( TryPerformAttack() )
				ConsumeAttackCooldown();
		}

		return;
	}

	if ( !CanMoveNow() )
	{
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		return;
	}

	if ( dt <= 0.0f )
	{
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		return;
	}

	const float moveDistance = GetChaseMoveSpeed() * dt;
	if ( moveDistance <= 0.0f )
	{
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		return;
	}

	SetMonsterLocomotionState(GetChaseLocomotionState());
	MoveDirectNoNavTowards(m_pTarget->GetPosition(), moveDistance);
}

bool CBossStageMonsterAIComponent::CanStartAttackAgainstTarget() const
{
	// 5번 메가그리드는 뻥 뚫린 구조로 취급한다.
	// 따라서 BowMan도 navmesh line-of-sight 판정을 하지 않는다.
	return true;
}

bool CBossStageMonsterAIComponent::TryPerformAttack()
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

EMonsterAnimState CBossStageMonsterAIComponent::GetChaseLocomotionState() const
{
	return EMonsterAnimState::Run;
}

EMonsterAnimState CBossStageMonsterAIComponent::GetWalkLocomotionState() const
{
	return EMonsterAnimState::Move;
}

bool CBossStageMonsterAIComponent::IsPlayerValidBossStageTarget(CGameObject* player) const
{
	if ( !player )
		return false;

	if ( !m_pScene )
		return false;

	bool isRegisteredPlayer = false;

	for ( int slot = 0; slot < 4; ++slot )
	{
		if ( m_pScene->GetPlayerBySlot(slot) == player )
		{
			isRegisteredPlayer = true;
			break;
		}
	}

	if ( !isRegisteredPlayer )
		return false;

	if ( auto* hp = player->GetComponent<CHealthComponent>() )
	{
		if ( hp->IsDead() )
			return false;
	}

	return m_pScene->IsPlayerInsideBossStageBattleArea(player);
}

bool CBossStageMonsterAIComponent::HasAnyValidPlayerInsideBossStage() const
{
	if ( !m_pScene )
		return false;

	for ( int slot = 0; slot < 4; ++slot )
	{
		if ( IsPlayerValidBossStageTarget(m_pScene->GetPlayerBySlot(slot)) )
			return true;
	}

	return false;
}

CGameObject* CBossStageMonsterAIComponent::FindNearestPlayerInsideBossStage() const
{
	if ( !m_pScene )
		return nullptr;

	CGameObject* owner = GetOwner();
	if ( !owner )
		return nullptr;

	CGameObject* bestPlayer = nullptr;
	float bestDistSq = FLT_MAX;

	const XMFLOAT3 ownerPos = owner->GetPosition();

	for ( int slot = 0; slot < 4; ++slot )
	{
		CGameObject* player = m_pScene->GetPlayerBySlot(slot);
		if ( !IsPlayerValidBossStageTarget(player) )
			continue;

		const float distSq =
			DistanceSqXZLocal(ownerPos, player->GetPosition());

		if ( distSq < bestDistSq )
		{
			bestDistSq = distSq;
			bestPlayer = player;
		}
	}

	return bestPlayer;
}

bool CBossStageMonsterAIComponent::MoveDirectNoNavTowards(
	const XMFLOAT3& targetPos,
	float maxStepDistance)
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return false;

	if ( maxStepDistance <= 0.0f )
		return false;

	const XMFLOAT3 pos = owner->GetPosition();

	XMFLOAT3 delta(
		targetPos.x - pos.x,
		0.0f,
		targetPos.z - pos.z
	);

	const float lenSq = delta.x * delta.x + delta.z * delta.z;
	if ( lenSq <= 1.0e-8f )
		return false;

	const float len = std::sqrt(lenSq);
	const float invLen = 1.0f / len;

	XMFLOAT3 dir(
		delta.x * invLen,
		0.0f,
		delta.z * invLen
	);

	const float stepDistance =
		( maxStepDistance < len ) ? maxStepDistance : len;

	return MoveDirectNoNavByDirection(
		dir,
		stepDistance
	);
}

bool CBossStageMonsterAIComponent::MoveDirectNoNavByDirection(
	const XMFLOAT3& direction,
	float maxStepDistance)
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return false;

	if ( maxStepDistance <= 0.0f )
		return false;

	XMFLOAT3 dir(direction.x, 0.0f, direction.z);

	const float lenSq = dir.x * dir.x + dir.z * dir.z;
	if ( lenSq <= 1.0e-8f )
		return false;

	const float invLen = 1.0f / std::sqrt(lenSq);
	dir.x *= invLen;
	dir.z *= invLen;

	const XMFLOAT3 pos = owner->GetPosition();

	const XMFLOAT3 lookTarget(
		pos.x + dir.x,
		pos.y,
		pos.z + dir.z
	);

	FaceTowardsNoClamp(lookTarget);

	XMFLOAT3 newPos(
		pos.x + dir.x * maxStepDistance,
		pos.y,
		pos.z + dir.z * maxStepDistance
	);

	owner->SetPosition(newPos);

	if ( auto* collider = owner->GetComponent<CColliderComponent>() )
		collider->UpdateWorldBounds();

	return true;
}

//-----------------------------------------------------------------------------
// SwordMan
//-----------------------------------------------------------------------------
CSwordManAIComponent::CSwordManAIComponent(CGameObject* owner)
	: CMonsterAIComponent(owner)
{
	SetMoveSpeeds(4.0f, 8.0f);
	SetRepathInterval(0.35f);
	SetPathPointReachDistance(0.20f);
	SetGoalReachDistance(0.85f);

	SetChaseRanges(35.0f, 50.0f);
	SetAttackRange(3.0f);

	SetAttackCooldown(1.0f);

	SetPatrolEnabled(true);
	SetPatrolHalfDistance(5.0f);
	SetPatrolTurnSpeedDegrees(240.0f);
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
	SetMoveSpeeds(4.0f, 8.0f);
	SetRepathInterval(0.35f);
	SetPathPointReachDistance(0.20f);
	SetGoalReachDistance(0.85f);

	SetDetectRange(50.0f);

	SetChaseRanges(50.0f, 50.0f);
	SetAttackRange(25.0f);

	SetAttackCooldown(1.0f);

	SetPatrolEnabled(true);
	SetPatrolHalfDistance(5.0f);
	SetPatrolTurnSpeedDegrees(240.0f);
}

bool CBowManAIComponent::CanStartAttackAgainstTarget() const
{
	CGameObject* target = GetTarget();
	if ( !target )
		return false;

	return HasDirectNavMeshLineTo(target->GetPosition(), false);
}

bool CBowManAIComponent::TryPerformAttack()
{
	if ( !CanStartAttackAgainstTarget() )
		return false;

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
	SetMoveSpeeds(5.0f, 12.0f);
	SetRepathInterval(0.35f);
	SetPathPointReachDistance(0.20f);
	SetGoalReachDistance(0.85f);

	SetChaseRanges(25.0f, 50.0f); 
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
	// 보스는 run 애니메이션 없이 walk/move 계열로만 이동한다.
	// 크기가 2.5배라 8.0은 체감 속도가 과하게 빠를 수 있으므로 6.0부터 시작한다.
	SetMoveSpeeds(6.0f, 6.0f);
	SetChaseRunAnimationEnabled(false);

	SetRepathInterval(0.35f);
	SetPathPointReachDistance(0.35f);
	SetGoalReachDistance(1.25f);

	// 보스는 chase range로 target을 끊지 않는다.
	// 실제 추적 가능 여부는 5번 메가그리드 중앙 진입 여부로만 판단한다.
	SetChaseRanges(1000000.0f, 1000000.0f);

	// 보스 크기 2.5배 기준 근접 행동 판정 범위.
	SetAttackRange(7.0f);

	// base cooldown은 boss global action cooldown과 맞춰둔다.
	SetAttackCooldown(0.8f);

	m_postAttackMoveLockDuration = 0.0f;

	m_bossMeleeRange = 7.0f;
	m_bossPreferredSpellRange = 12.0f;

	m_bossGlobalActionCooldown = 0.8f;
	m_bossMeleeCooldown = 2.0f;
	m_bossSpellCooldown = 3.5f;

	m_bossGlobalActionCooldownRemaining = 0.0f;
	m_bossMeleeCooldownRemaining = 0.0f;
	m_bossSpellCooldownRemaining = 0.0f;

	m_bBossWasMeleeActionPlaying = false;
	m_bBossHitReactionPolicyConfigured = false;

	m_bBossCombatAIEnabled = true;
	m_bBossSummonEnabled = true;

	m_bBossOpeningSpellPending = true;
	m_bBossOpeningSpellRequested = false;
	m_bossOpeningSpellRequestAgeSec = 0.0f;

	m_bBossPostMeleeEvading = false;

	m_bossMeleeAttackForward =
		XMFLOAT3(0.0f, 0.0f, 1.0f);

	m_bossPostMeleeEvadeDirection =
		XMFLOAT3(0.0f, 0.0f, 0.0f);

	m_bossPostMeleeEvadeTarget =
		XMFLOAT3(0.0f, 0.0f, 0.0f);

	m_bossPostMeleeEvadeRemainingDistance = 0.0f;

	m_bossPostMeleeTurnSpeedDegrees = 900.0f;

	m_bossSpellTurnSpeedDegrees = 720.0f;

	m_bossCallTurnSpeedDegrees = 720.0f;
	ResetBossCallState();
}

void CBossAIComponent::ConfigureBossSimulation(
	bool combatAIEnabled,
	bool summonEnabled)
{
	m_bBossCombatAIEnabled = combatAIEnabled;

	// combat AI가 켜지면 summon은 항상 켜져야 한다.
	m_bBossSummonEnabled = summonEnabled || combatAIEnabled;

	if ( !m_bBossCombatAIEnabled )
	{
		ClearTarget();
		ClearPath();

		m_bBossOpeningSpellRequested = false;
		m_bossOpeningSpellRequestAgeSec = 0.0f;

		m_bBossWasMeleeActionPlaying = false;
		m_bBossPostMeleeEvading = false;
		m_bossPostMeleeEvadeRemainingDistance = 0.0f;
	}
}

void CBossAIComponent::OnUpdate(float dt)
{
	// 전투 AI가 켜진 일반 보스는 기존 base 흐름을 그대로 쓴다.
	if ( m_bBossCombatAIEnabled )
	{
		CMonsterAIComponent::OnUpdate(dt);
		return;
	}

	if ( !m_bAIEnabled )
		return;

	CGameObject* owner = GetOwner();
	if ( !owner )
		return;

	EnsureHomeTransformCaptured();

	if ( IsObjectDeadByHealthLocal(owner) )
	{
		ClearTarget();
		ClearPath();
		ClearReturnHomePath();
		return;
	}

	if ( !m_pScene )
		return;

	ConfigureBossHitReactionPolicy();

	UpdateBossCooldowns(dt);

	if ( m_bBossSummonEnabled )
		UpdateBossCallThresholdState();

	// summon-only 모드에서는 추적/공격 target을 절대 유지하지 않는다.
	ClearTarget();
	ClearPath();
	SetMonsterLocomotionState(EMonsterAnimState::Idle);

	if ( !m_bBossSummonEnabled )
		return;

	// target 없이도 상승 -> Call -> 하강이 가능해야 한다.
	if ( UpdateBossCallSequence(dt, nullptr, true) )
		return;

	SetMonsterLocomotionState(EMonsterAnimState::Idle);
}

void CBossAIComponent::ResetBossCallState()
{
	m_bossCallThresholdMask = 0;
	m_bossPendingCallCount = 0;
	m_bossExecutedCallCount = 0;

	m_bBossCallCommandRequested = false;
	m_bBossCallConsumePendingOnStart = false;
	m_bossCallRequestAgeSec = 0.0f;

	m_bBossCallMonsterSpawnPending = false;
	m_bossCallMonsterSpawnPendingCallIndex = -1;
	m_bossCallMonsterSpawnDelaySec = 0.0f;
	m_bossCallMonsterSpawnElapsedSec = 0.0f;

	m_bBossCallRising = false;
	m_bBossCallRiseCompletedForCurrentCall = false;
	m_bBossCallDescendPendingOnCallEnd = false;
	m_bBossCallDescending = false;

	m_bossCallBaseY = 0.0f;

	m_bossCallRiseStartY = 0.0f;
	m_bossCallRiseElapsedSec = 0.0f;

	m_bossCallDescendStartY = 0.0f;
	m_bossCallDescendElapsedSec = 0.0f;
}

void CBossAIComponent::ResetBossOpeningSpellState()
{
	m_bBossOpeningSpellPending = true;
	m_bBossOpeningSpellRequested = false;
	m_bossOpeningSpellRequestAgeSec = 0.0f;
}

bool CBossAIComponent::AcquireTarget()
{
	CGameScene* scene = GetScene();
	if ( !scene )
		return false;

	if ( !scene->IsLocalMonsterChaseEnabled() )
		return false;

	for ( int slot = 0; slot < 4; ++slot )
	{
		CGameObject* player = scene->GetPlayerBySlot(slot);
		if ( !player )
			continue;

		if ( auto* hp = player->GetComponent<CHealthComponent>() )
		{
			if ( hp->IsDead() )
				continue;
		}

		if ( !IsPlayerInsideBossBattleZone(player) )
			continue;

		SetTarget(player);
		return true;
	}

	return false;
}

void CBossAIComponent::ConfigureBossHitReactionPolicy()
{
	if ( m_bBossHitReactionPolicyConfigured )
		return;

	auto* ctrl = GetMonsterAnimController();

	if ( !ctrl )
		return;

	// 보스:
	// 1) action 중에는 Hit 애니메이션으로 캔슬되지 않음.
	// 2) Hit 애니메이션이 한번 허용되면 일정 시간 동안 추가 Hit 애니메이션 무시.
	// 3) 이건 애니메이션만 막는 슈퍼아머이며, 대미지는 그대로 받음.
	ctrl->SetHitReactionPolicy(
		true,
		kBossHitReactionAnimSuperArmorSec
	);

	m_bBossHitReactionPolicyConfigured = true;
}

void CBossAIComponent::UpdateBossCallThresholdState()
{
	if ( !m_bBossSummonEnabled )
		return;

	CGameObject* owner = GetOwner();
	if ( !owner )
		return;

	auto* hp = owner->GetComponent<CHealthComponent>();
	if ( !hp )
		return;

	if ( hp->IsDead() )
		return;

	const int maxHp = hp->GetMaxHp();
	const int currentHp = hp->GetCurrentHp();

	if ( maxHp <= 0 )
		return;

	auto CheckThreshold =
		[ & ](
			uint8_t bit,
			int numerator
		)
		{
			const uint8_t mask =
				static_cast< uint8_t >( 1u << bit );

			if ( ( m_bossCallThresholdMask & mask ) != 0 )
				return;

			// current / max <= numerator / 4
			// float 오차 방지를 위해 정수 비교.
			const int64_t lhs =
				static_cast< int64_t >( currentHp ) * 4;

			const int64_t rhs =
				static_cast< int64_t >( maxHp ) * numerator;

			if ( lhs <= rhs )
			{
				m_bossCallThresholdMask |= mask;
				++m_bossPendingCallCount;
			}
		};

	// 실제 전투 중 순서:
	// 3/4 이하 -> 2/4 이하 -> 1/4 이하
	CheckThreshold(0, 3);
	CheckThreshold(1, 2);
	CheckThreshold(2, 1);
}

bool CBossAIComponent::HasPendingBossCall() const
{
	return m_bossPendingCallCount > 0;
}

bool CBossAIComponent::IsBossCallVerticalSequenceActive() const
{
	return
		m_bBossCallRising ||
		m_bBossCallDescending ||
		m_bBossCallDescendPendingOnCallEnd;
}

void CBossAIComponent::BeginBossCallRise()
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return;

	const XMFLOAT3 pos = owner->GetPosition();

	m_bossCallBaseY = pos.y;

	m_bossCallRiseStartY = pos.y;
	m_bossCallRiseElapsedSec = 0.0f;

	m_bBossCallRising = true;
	m_bBossCallRiseCompletedForCurrentCall = false;
	m_bBossCallDescending = false;
	m_bBossCallDescendPendingOnCallEnd = false;

	if ( CGameScene* scene = GetScene() )
	{
		const int nextCallIndex = m_bossExecutedCallCount + 1;

		scene->BeginBossCallMonsterSummonVisuals(
			nextCallIndex,
			kBossCallRiseDuration
		);
	}

	ClearPath();
	SetMonsterLocomotionState(EMonsterAnimState::Idle);
}

bool CBossAIComponent::UpdateBossCallRise(float dt)
{
	if ( !m_bBossCallRising )
		return false;

	CGameObject* owner = GetOwner();
	if ( !owner )
	{
		m_bBossCallRising = false;
		return false;
	}

	ClearPath();
	SetMonsterLocomotionState(EMonsterAnimState::Idle);

	if ( dt > 0.0f )
		m_bossCallRiseElapsedSec += dt;

	const float duration =
		( kBossCallRiseDuration > 0.0f )
		? kBossCallRiseDuration
		: 0.001f;

	const float t =
		std::clamp(
			m_bossCallRiseElapsedSec / duration,
			0.0f,
			1.0f
		);

	const float easedT = SmoothStep01Local(t);

	XMFLOAT3 pos = owner->GetPosition();
	pos.y = m_bossCallRiseStartY + kBossCallLiftHeight * easedT;
	owner->SetPosition(pos);

	if ( auto* collider = owner->GetComponent<CColliderComponent>() )
		collider->UpdateWorldBounds();

	if ( t >= 1.0f )
	{
		pos.y = m_bossCallBaseY + kBossCallLiftHeight;
		owner->SetPosition(pos);

		if ( auto* collider = owner->GetComponent<CColliderComponent>() )
			collider->UpdateWorldBounds();

		m_bBossCallRising = false;
		m_bBossCallRiseCompletedForCurrentCall = true;
		m_bossCallRiseElapsedSec = 0.0f;

		// 이번 프레임에 바로 Call 요청 단계로 넘어갈 수 있게 false 반환.
		return false;
	}

	return true;
}

void CBossAIComponent::BeginBossCallDescend()
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return;

	const XMFLOAT3 pos = owner->GetPosition();

	m_bossCallDescendStartY = pos.y;
	m_bossCallDescendElapsedSec = 0.0f;

	m_bBossCallDescending = true;
	m_bBossCallDescendPendingOnCallEnd = false;

	ClearPath();
	SetMonsterLocomotionState(EMonsterAnimState::Idle);
}

bool CBossAIComponent::UpdateBossCallDescend(float dt)
{
	if ( !m_bBossCallDescending )
		return false;

	CGameObject* owner = GetOwner();
	if ( !owner )
	{
		m_bBossCallDescending = false;
		return false;
	}

	ClearPath();
	SetMonsterLocomotionState(EMonsterAnimState::Idle);

	if ( dt > 0.0f )
		m_bossCallDescendElapsedSec += dt;

	const float duration =
		( kBossCallDescendDuration > 0.0f )
		? kBossCallDescendDuration
		: 0.001f;

	const float t =
		std::clamp(
			m_bossCallDescendElapsedSec / duration,
			0.0f,
			1.0f
		);

	// 등속 하강.
	XMFLOAT3 pos = owner->GetPosition();
	pos.y =
		m_bossCallDescendStartY +
		( m_bossCallBaseY - m_bossCallDescendStartY ) * t;

	owner->SetPosition(pos);

	if ( auto* collider = owner->GetComponent<CColliderComponent>() )
		collider->UpdateWorldBounds();

	if ( t >= 1.0f )
	{
		pos.y = m_bossCallBaseY;
		owner->SetPosition(pos);

		if ( auto* collider = owner->GetComponent<CColliderComponent>() )
			collider->UpdateWorldBounds();

		m_bBossCallDescending = false;
		m_bossCallDescendElapsedSec = 0.0f;
	}

	return true;
}

void CBossAIComponent::UpdateBehavior(float dt)
{
	ConfigureBossHitReactionPolicy();

	UpdateBossCooldowns(dt);
	UpdateBossCallThresholdState();

	if ( !HasValidTarget() )
	{
		if ( m_bBossOpeningSpellPending )
		{
			m_bBossOpeningSpellRequested = false;
			m_bossOpeningSpellRequestAgeSec = 0.0f;
		}

		ClearPath();
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		return;
	}

	CGameScene* scene = GetScene();

	if ( scene && !scene->IsLocalMonsterChaseEnabled() )
	{
		BeginReturnHome();
		return;
	}

	CGameObject* target = GetTarget();
	if ( !IsPlayerInsideBossBattleZone(target) )
	{
		if ( m_bBossOpeningSpellPending )
		{
			m_bBossOpeningSpellRequested = false;
			m_bossOpeningSpellRequestAgeSec = 0.0f;
		}

		BeginReturnHome();
		return;
	}

	if ( UpdateBossCallRise(dt) )
	{
		return;
	}

	if ( m_bBossCallDescendPendingOnCallEnd && !IsBossCallActionPlaying() )
	{
		BeginBossCallDescend();
	}

	if ( UpdateBossCallDescend(dt) )
	{
		return;
	}

	const float distanceToTarget = GetDistanceToTargetXZ();

	if ( UpdateBossCallSequence(dt, target, false) )
	{
		return;
	}

	const bool meleeActionPlaying = IsBossMeleeActionPlaying();

	if ( meleeActionPlaying )
	{
		m_bBossWasMeleeActionPlaying = true;

		ClearPath();

		// 근거리 공격 중에는 이동/회전 모두 금지.
		// action clip은 MonsterAnimController가 유지하므로 locomotion state를 덮지 않는다.
		return;
	}

	if ( m_bBossWasMeleeActionPlaying )
	{
		m_bBossWasMeleeActionPlaying = false;
		BeginBossPostMeleeEvade();
	}

	if ( IsBossSpellActionPlaying() )
	{
		ClearPath();

		if ( m_bBossOpeningSpellPending )
		{
			m_bBossOpeningSpellPending = false;
			m_bBossOpeningSpellRequested = false;
			m_bossOpeningSpellRequestAgeSec = 0.0f;

			ConsumeBossSpellCooldown();
		}

		// 원거리 공격 중에는 이동하지 않고, 플레이어 방향으로만 부드럽게 회전한다.
		SmoothFaceTowardsTarget(
			target,
			dt,
			m_bossSpellTurnSpeedDegrees
		);

		return;
	}

	if ( UpdateBossPostMeleeEvade(dt) )
	{
		return;
	}

	const bool canStartAction = CanStartBossAction();

	if ( HasPendingBossCall() )
	{
		if ( UpdateBossCallSequence(dt, target, true) )
			return;

		return;
	}

	if ( m_bBossOpeningSpellPending )
	{
		ClearPath();

		// 중요:
		// 이미 Spell 요청을 한 뒤에는 SetMonsterLocomotionState(Idle)을 다시 호출하지 않는다.
		// Spell 전환 대기 중에 Idle을 계속 밀어 넣으면 Spell 전환이 덮여서
		// spellPlaying=0 상태로 굳을 수 있다.
		if ( m_bBossOpeningSpellRequested )
		{
			m_bossOpeningSpellRequestAgeSec += dt;

			if ( target )
			{
				SmoothFaceTowardsTarget(
					target,
					dt,
					m_bossSpellTurnSpeedDegrees
				);
			}

			if ( m_bossOpeningSpellRequestAgeSec >= 0.50f )
			{
				// 0.5초 동안 SpellPhase로 안 들어갔고 현재 action lock도 없다면,
				// 이전 요청은 컨트롤러에서 소비되지 않은 것으로 보고 다시 시도하게 한다.
				if ( !IsAIActionLockedByAnimation() && !IsBossSpellActionPlaying() )
				{
					m_bBossOpeningSpellRequested = false;
				}

				m_bossOpeningSpellRequestAgeSec = 0.0f;
			}

			return;
		}

		if ( !canStartAction )
		{
			SetMonsterLocomotionState(EMonsterAnimState::Idle);

			if ( target )
			{
				SmoothFaceTowardsTarget(
					target,
					dt,
					m_bossSpellTurnSpeedDegrees
				);
			}

			static float s_openingBlockedLogAge = 0.0f;
			s_openingBlockedLogAge += dt;

			if ( s_openingBlockedLogAge >= 0.50f )
			{
				s_openingBlockedLogAge = 0.0f;
			}

			return;
		}

		// 여기서만 Idle을 세팅한다.
		// 즉, Spell 요청 직전 1회만 locomotion을 정리한다.
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		FaceTowards(target->GetPosition());

		const bool requestResult = TryPerformSpellAttack();

		if ( requestResult )
		{
			m_bBossOpeningSpellRequested = true;
			m_bossOpeningSpellRequestAgeSec = 0.0f;
		}
		
		return;
	}

	if ( canStartAction )
	{
		if ( distanceToTarget <= m_bossMeleeRange )
		{
			ClearPath();
			SetMonsterLocomotionState(EMonsterAnimState::Idle);

			if ( m_bossMeleeCooldownRemaining <= 0.0f )
			{
				// 공격 시작 직전에는 일단 플레이어를 향하게 한다.
				// 공격 중에는 IsBossMeleeActionPlaying() 분기에서 회전이 막힌다.
				FaceTowards(target->GetPosition());

				if ( TryPerformMeleeAttack() )
					ConsumeBossMeleeCooldown();

				return;
			}

			// 근거리지만 melee cooldown 중이면 부드럽게 바라보기만 한다.
			SmoothFaceTowardsTarget(
				target,
				dt,
				m_bossPostMeleeTurnSpeedDegrees
			);

			return;
		}
		else
		{
			if ( m_bossSpellCooldownRemaining <= 0.0f )
			{
				ClearPath();
				SetMonsterLocomotionState(EMonsterAnimState::Idle);
				FaceTowards(target->GetPosition());

				if ( TryPerformSpellAttack() )
					ConsumeBossSpellCooldown();

				return;
			}
		}
	}

	if ( distanceToTarget <= m_bossMeleeRange )
	{
		ClearPath();

		if ( !IsAIActionLockedByAnimation() )
			SetMonsterLocomotionState(EMonsterAnimState::Idle);

		SmoothFaceTowardsTarget(
			target,
			dt,
			m_bossPostMeleeTurnSpeedDegrees
		);

		return;
	}

	if ( !CanMoveNow() )
	{
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		return;
	}

	if ( TryMoveDirectlyToTarget(dt) )
		return;

	if ( ShouldRepath() || !HasPath() )
		RebuildPathToTarget();

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

bool CBossAIComponent::TryPerformAttack()
{
	if ( m_pendingAttackIntent == EBossAttackIntent::Call )
		return TryPerformCall();

	if ( m_pendingAttackIntent == EBossAttackIntent::Spell )
		return TryPerformSpellAttack();

	return TryPerformMeleeAttack();
}

bool CBossAIComponent::CanMoveNow() const
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return false;

	if ( auto* hp = owner->GetComponent<CHealthComponent>() )
	{
		if ( hp->IsDead() )
			return false;
	}

	// 근거리 Attack / 원거리 Spell 중에는 이동하지 않는다.
	if ( IsAIActionLockedByAnimation() )
		return false;

	// 근거리 공격 후 회피 이동 중에는 일반 추적 이동을 막는다.
	// 회피 이동 자체는 UpdateBossPostMeleeEvade()에서 직접 처리한다.
	if ( m_bBossPostMeleeEvading )
		return false;

	// Call 전 상승 / Call 후 하강 중에는 일반 이동 금지.
	if ( IsBossCallVerticalSequenceActive() )
		return false;

	return true;
}

bool CBossAIComponent::CanThinkNow() const
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return false;

	if ( auto* hp = owner->GetComponent<CHealthComponent>() )
	{
		if ( hp->IsDead() )
			return false;
	}

	// 보스는 공격/스펠 애니메이션 중에도 target 유지, 전투구역 체크, 이동 판단을 계속한다.
	return true;
}

bool CBossAIComponent::CanRotateNow() const
{
	CGameObject* owner = GetOwner();
	if ( !owner )
		return false;

	if ( auto* hp = owner->GetComponent<CHealthComponent>() )
	{
		if ( hp->IsDead() )
			return false;
	}

	// 근거리 공격 중에는 회전 금지.
	if ( IsBossMeleeActionPlaying() )
		return false;

	// Call 전 상승 / Call 후 하강 중에도 회전 금지.
	// Idle 상태로 수직 연출만 수행한다.
	if ( IsBossCallVerticalSequenceActive() )
		return false;

	// Call 중에도 회전 금지.
	// Call은 소환 연출용 action이므로 도중에 회전으로 비틀리지 않게 둔다.
	if ( IsBossCallActionPlaying() )
		return false;

	// Spell 중에는 회전 가능.
	// 일반 추적/대기 중에도 회전 가능.
	return true;
}

bool CBossAIComponent::IsBossMeleeActionPlaying() const
{
	auto* ctrl = GetMonsterAnimController();
	if ( !ctrl )
		return false;

	return
		ctrl->IsAttackPrimaryPhase() ||
		ctrl->IsAttackChainPhase();
}

bool CBossAIComponent::IsBossSpellActionPlaying() const
{
	auto* ctrl = GetMonsterAnimController();
	if ( !ctrl )
		return false;

	return ctrl->IsSpellPhase();
}

bool CBossAIComponent::IsBossCallActionPlaying() const
{
	auto* ctrl = GetMonsterAnimController();
	if ( !ctrl )
		return false;

	return ctrl->IsCallPhase();
}

bool CBossAIComponent::SmoothFaceTowardsTarget(
	CGameObject* target,
	float dt,
	float turnSpeedDegreesPerSec)
{
	if ( !target )
		return false;

	if ( dt <= 0.0f )
		return false;

	if ( turnSpeedDegreesPerSec <= 0.0f )
		return false;

	if ( !CanRotateNow() )
		return false;

	CGameObject* owner = GetOwner();
	if ( !owner )
		return false;

	const float targetYaw =
		ComputeYawDegreesToPointXZ(
			owner->GetPosition(),
			target->GetPosition()
		);

	return RotateOwnerYawTowards(
		targetYaw,
		turnSpeedDegreesPerSec * dt
	);
}

void CBossAIComponent::CaptureBossMeleeAttackForward()
{
	CGameObject* owner = GetOwner();

	if ( !owner )
	{
		m_bossMeleeAttackForward =
			XMFLOAT3(0.0f, 0.0f, 1.0f);
		return;
	}

	const float yawDeg = GetOwnerYawDegrees();
	const float yawRad = XMConvertToRadians(yawDeg);

	XMFLOAT3 forward(
		std::sin(yawRad),
		0.0f,
		std::cos(yawRad)
	);

	const float lenSq =
		forward.x * forward.x +
		forward.z * forward.z;

	if ( lenSq <= 1.0e-8f )
	{
		m_bossMeleeAttackForward =
			XMFLOAT3(0.0f, 0.0f, 1.0f);
		return;
	}

	const float invLen = 1.0f / std::sqrt(lenSq);

	forward.x *= invLen;
	forward.y = 0.0f;
	forward.z *= invLen;

	m_bossMeleeAttackForward = forward;
}

XMFLOAT3 CBossAIComponent::ClampBossPostMeleeEvadePointToStage(
	const XMFLOAT3& p) const
{
	const float minX =
		kBossPostMeleeEvadeStageCenterX -
		kBossPostMeleeEvadeStageHalfExtent +
		kBossPostMeleeEvadeStagePadding;

	const float maxX =
		kBossPostMeleeEvadeStageCenterX +
		kBossPostMeleeEvadeStageHalfExtent -
		kBossPostMeleeEvadeStagePadding;

	const float minZ =
		kBossPostMeleeEvadeStageCenterZ -
		kBossPostMeleeEvadeStageHalfExtent +
		kBossPostMeleeEvadeStagePadding;

	const float maxZ =
		kBossPostMeleeEvadeStageCenterZ +
		kBossPostMeleeEvadeStageHalfExtent -
		kBossPostMeleeEvadeStagePadding;

	XMFLOAT3 out = p;

	if ( out.x < minX ) out.x = minX;
	if ( out.x > maxX ) out.x = maxX;
	if ( out.z < minZ ) out.z = minZ;
	if ( out.z > maxZ ) out.z = maxZ;

	return out;
}

bool CBossAIComponent::IsBossPostMeleeEvadeDestinationValid(
	const XMFLOAT3& from,
	const XMFLOAT3& dir) const
{
	const XMFLOAT3 dst(
		from.x + dir.x * kBossPostMeleeEvadeDistance,
		from.y,
		from.z + dir.z * kBossPostMeleeEvadeDistance
	);

	const float minX =
		kBossPostMeleeEvadeStageCenterX -
		kBossPostMeleeEvadeStageHalfExtent +
		kBossPostMeleeEvadeStagePadding;

	const float maxX =
		kBossPostMeleeEvadeStageCenterX +
		kBossPostMeleeEvadeStageHalfExtent -
		kBossPostMeleeEvadeStagePadding;

	const float minZ =
		kBossPostMeleeEvadeStageCenterZ -
		kBossPostMeleeEvadeStageHalfExtent +
		kBossPostMeleeEvadeStagePadding;

	const float maxZ =
		kBossPostMeleeEvadeStageCenterZ +
		kBossPostMeleeEvadeStageHalfExtent -
		kBossPostMeleeEvadeStagePadding;

	if ( dst.x < minX || dst.x > maxX )
		return false;

	if ( dst.z < minZ || dst.z > maxZ )
		return false;

	return true;
}

bool CBossAIComponent::SelectBossPostMeleeEvadeDirection(
	XMFLOAT3& outDir) const
{
	CGameObject* owner = GetOwner();

	if ( !owner )
	{
		outDir = XMFLOAT3(0.0f, 0.0f, 1.0f);
		return false;
	}

	XMFLOAT3 forward = m_bossMeleeAttackForward;

	float forwardLenSq =
		forward.x * forward.x +
		forward.z * forward.z;

	if ( forwardLenSq <= 1.0e-8f )
	{
		forward = XMFLOAT3(0.0f, 0.0f, 1.0f);
		forwardLenSq = 1.0f;
	}

	const float invForwardLen = 1.0f / std::sqrt(forwardLenSq);

	forward.x *= invForwardLen;
	forward.y = 0.0f;
	forward.z *= invForwardLen;

	const XMFLOAT3 right(
		forward.z,
		0.0f,
		-forward.x
	);

	const XMFLOAT3 left(
		-right.x,
		0.0f,
		-right.z
	);

	const XMFLOAT3 back(
		-forward.x,
		0.0f,
		-forward.z
	);

	const XMFLOAT3 pos = owner->GetPosition();

	std::array<XMFLOAT3, 3> candidates =
	{
		left,
		right,
		back
	};

	std::array<int, 3> order = { 0, 1, 2 };

	static std::mt19937 rng{ std::random_device{}( ) };
	std::shuffle(order.begin(), order.end(), rng);

	for ( int idx : order )
	{
		const XMFLOAT3& candidate = candidates[idx];

		if ( IsBossPostMeleeEvadeDestinationValid(pos, candidate) )
		{
			outDir = candidate;
			return true;
		}
	}

	// 좌/우/뒤가 모두 막힌 경우에만 예외적으로 앞으로 이동.
	outDir = forward;
	return false;
}

void CBossAIComponent::BeginBossPostMeleeEvade()
{
	CGameObject* owner = GetOwner();

	if ( !owner )
		return;

	XMFLOAT3 evadeDir{};
	SelectBossPostMeleeEvadeDirection(evadeDir);

	const XMFLOAT3 pos = owner->GetPosition();

	XMFLOAT3 rawTarget(
		pos.x + evadeDir.x * kBossPostMeleeEvadeDistance,
		pos.y,
		pos.z + evadeDir.z * kBossPostMeleeEvadeDistance
	);

	// 좌/우/뒤가 막혀서 앞으로 가는 경우도 stage 밖으로 튀지 않게 최종 clamp.
	const XMFLOAT3 target =
		ClampBossPostMeleeEvadePointToStage(rawTarget);

	XMFLOAT3 delta(
		target.x - pos.x,
		0.0f,
		target.z - pos.z
	);

	const float distSq =
		delta.x * delta.x +
		delta.z * delta.z;

	if ( distSq <= 1.0e-6f )
	{
		m_bBossPostMeleeEvading = false;
		m_bossPostMeleeEvadeRemainingDistance = 0.0f;
		return;
	}

	const float dist = std::sqrt(distSq);
	const float invDist = 1.0f / dist;

	m_bossPostMeleeEvadeDirection =
		XMFLOAT3(
			delta.x * invDist,
			0.0f,
			delta.z * invDist
		);

	m_bossPostMeleeEvadeTarget = target;
	m_bossPostMeleeEvadeRemainingDistance = dist;
	m_bBossPostMeleeEvading = true;

	ClearPath();
	SetMonsterLocomotionState(EMonsterAnimState::Idle);
}

bool CBossAIComponent::UpdateBossPostMeleeEvade(float dt)
{
	if ( !m_bBossPostMeleeEvading )
		return false;

	CGameObject* owner = GetOwner();

	if ( !owner )
	{
		m_bBossPostMeleeEvading = false;
		m_bossPostMeleeEvadeRemainingDistance = 0.0f;
		return false;
	}

	CGameObject* target = GetTarget();

	ClearPath();
	SetMonsterLocomotionState(EMonsterAnimState::Idle);

	// 회피 이동 중에도 플레이어를 계속 바라본다.
	if ( target )
	{
		SmoothFaceTowardsTarget(
			target,
			dt,
			m_bossPostMeleeTurnSpeedDegrees
		);
	}

	if ( dt <= 0.0f )
		return true;

	const float maxStep =
		kBossPostMeleeEvadeSpeed * dt;

	float step = maxStep;

	if ( step > m_bossPostMeleeEvadeRemainingDistance )
		step = m_bossPostMeleeEvadeRemainingDistance;

	if ( step <= 0.0f )
	{
		m_bBossPostMeleeEvading = false;
		m_bossPostMeleeEvadeRemainingDistance = 0.0f;
		return true;
	}

	const XMFLOAT3 oldPos = owner->GetPosition();

	XMFLOAT3 newPos(
		oldPos.x + m_bossPostMeleeEvadeDirection.x * step,
		oldPos.y,
		oldPos.z + m_bossPostMeleeEvadeDirection.z * step
	);

	newPos = ClampBossPostMeleeEvadePointToStage(newPos);

	owner->SetPosition(newPos);

	if ( auto* collider = owner->GetComponent<CColliderComponent>() )
	{
		collider->UpdateWorldBounds();
	}

	const float movedDx = newPos.x - oldPos.x;
	const float movedDz = newPos.z - oldPos.z;

	const float movedDist =
		std::sqrt(movedDx * movedDx + movedDz * movedDz);

	m_bossPostMeleeEvadeRemainingDistance -= movedDist;

	if ( m_bossPostMeleeEvadeRemainingDistance <= 0.03f ||
		 movedDist <= 1.0e-5f )
	{
		owner->SetPosition(m_bossPostMeleeEvadeTarget);

		if ( auto* collider = owner->GetComponent<CColliderComponent>() )
		{
			collider->UpdateWorldBounds();
		}

		m_bBossPostMeleeEvading = false;
		m_bossPostMeleeEvadeRemainingDistance = 0.0f;
	}

	return true;
}

EMonsterAnimState CBossAIComponent::GetChaseLocomotionState() const
{
	return EMonsterAnimState::Idle;
}

EMonsterAnimState CBossAIComponent::GetWalkLocomotionState() const
{
	return EMonsterAnimState::Idle;
}

void CBossAIComponent::UpdateBossCooldowns(float dt)
{
	if ( dt <= 0.0f )
		return;

	if ( IsAIActionLockedByAnimation() )
		return;

	if ( m_bossGlobalActionCooldownRemaining > 0.0f )
	{
		m_bossGlobalActionCooldownRemaining -= dt;
		if ( m_bossGlobalActionCooldownRemaining < 0.0f )
			m_bossGlobalActionCooldownRemaining = 0.0f;
	}

	if ( m_bossMeleeCooldownRemaining > 0.0f )
	{
		m_bossMeleeCooldownRemaining -= dt;
		if ( m_bossMeleeCooldownRemaining < 0.0f )
			m_bossMeleeCooldownRemaining = 0.0f;
	}

	if ( m_bossSpellCooldownRemaining > 0.0f )
	{
		m_bossSpellCooldownRemaining -= dt;
		if ( m_bossSpellCooldownRemaining < 0.0f )
			m_bossSpellCooldownRemaining = 0.0f;
	}
}

bool CBossAIComponent::IsPlayerInsideBossBattleZone(CGameObject* player) const
{
	if ( !player )
		return false;

	CGameScene* scene = GetScene();

	if ( scene )
	{
		if ( !scene->IsLocalPlayer(player) )
			return false;

		return scene->IsPlayerInsideBossStageBattleArea(player);
	}

	return false;
}

bool CBossAIComponent::CanStartBossAction() const
{
	if ( IsBossCallVerticalSequenceActive() )
		return false;

	if ( IsAIActionLockedByAnimation() )
		return false;

	if ( m_bossGlobalActionCooldownRemaining > 0.0f )
		return false;

	return true;
}

bool CBossAIComponent::TryRequestPendingBossCall(CGameObject* target, float dt)
{
	ClearPath();

	if ( !m_bBossSummonEnabled )
		return false;

	if ( !HasPendingBossCall() )
		return false;

	if ( m_bBossCallRising || m_bBossCallDescending || m_bBossCallDescendPendingOnCallEnd )
	{
		SetMonsterLocomotionState(EMonsterAnimState::Idle);
		return true;
	}

	if ( m_bBossCallCommandRequested )
	{
		m_bossCallRequestAgeSec += dt;

		// Call 전환 대기 중에는 Idle을 계속 덮어쓰지 않는다.
		// 대신 action lock이 없을 때만 플레이어를 바라보게 한다.
		if ( target && !IsAIActionLockedByAnimation() )
		{
			SmoothFaceTowardsTarget(
				target,
				dt,
				m_bossCallTurnSpeedDegrees
			);
		}

		if ( m_bossCallRequestAgeSec >= 0.50f )
		{
			// 0.5초 동안 Call phase로 진입하지 못했고 현재 action lock도 없다면
			// 요청이 소비되지 않은 것으로 보고 재시도한다.
			if ( !IsAIActionLockedByAnimation() && !IsBossCallActionPlaying() )
			{
				m_bBossCallCommandRequested = false;
				m_bBossCallConsumePendingOnStart = false;
			}

			m_bossCallRequestAgeSec = 0.0f;
		}

		return true;
	}

	if ( !CanStartBossAction() )
	{
		// Hit 중이거나 다른 action lock 중이면 여기서 기다린다.
		// 즉, Hit 중 조건 충족 시 Hit 종료 후 상승 -> Call 실행.
		if ( !IsAIActionLockedByAnimation() )
			SetMonsterLocomotionState(EMonsterAnimState::Idle);

		if ( target && !IsAIActionLockedByAnimation() )
		{
			SmoothFaceTowardsTarget(
				target,
				dt,
				m_bossCallTurnSpeedDegrees
			);
		}

		return true;
	}

	SetMonsterLocomotionState(EMonsterAnimState::Idle);

	if ( target )
		FaceTowards(target->GetPosition());

	if ( !m_bBossCallRiseCompletedForCurrentCall )
	{
		BeginBossCallRise();
		return true;
	}

	if ( TryPerformCall() )
	{
		m_bBossCallCommandRequested = true;
		m_bBossCallConsumePendingOnStart = true;
		m_bossCallRequestAgeSec = 0.0f;
	}

	return true;
}

bool CBossAIComponent::TryPerformBossCommand(EMonsterAnimCommand command)
{
	auto* animComp = GetAnimatorComponent();
	if ( !animComp )
		return false;

	auto* ctrl = animComp->EnsureMonsterController();
	if ( !ctrl )
		return false;

	ctrl->RequestCommand(command);
	return true;
}

bool CBossAIComponent::TryPerformMeleeAttack()
{
	m_pendingAttackIntent = EBossAttackIntent::Melee;
	CaptureBossMeleeAttackForward();

	return TryPerformBossCommand(EMonsterAnimCommand::Attack);
}

bool CBossAIComponent::TryPerformSpellAttack()
{
	m_pendingAttackIntent = EBossAttackIntent::Spell;
	return TryPerformBossCommand(EMonsterAnimCommand::Spell);
}

bool CBossAIComponent::TryPerformCall()
{
	m_pendingAttackIntent = EBossAttackIntent::Call;
	return TryPerformBossCommand(EMonsterAnimCommand::Call);
}

void CBossAIComponent::ConsumeBossMeleeCooldown()
{
	m_bossGlobalActionCooldownRemaining = m_bossGlobalActionCooldown;
	m_bossMeleeCooldownRemaining = m_bossMeleeCooldown;

	// 실제 후처리는 melee action이 끝나는 순간
	// m_bossPostMeleeTurnRemaining으로 시작한다.
}

void CBossAIComponent::ConsumeBossSpellCooldown()
{
	m_bossGlobalActionCooldownRemaining = m_bossGlobalActionCooldown;
	m_bossSpellCooldownRemaining = m_bossSpellCooldown;

	// Spell은 공격 중 회전만 허용하고, 종료 후 별도 이동 잠금은 두지 않는다.
}

void CBossAIComponent::ConsumeBossCallCooldown()
{
	m_bossGlobalActionCooldownRemaining = m_bossGlobalActionCooldown;

	// Call은 HP 임계값 기반 1회성 예약 액션이므로 별도 Call cooldown은 두지 않는다.
	// 실제 소환은 이후 GameScene/Spawner 연동 단계에서 Call phase 중 타이밍을 잡아 처리하면 된다.
}