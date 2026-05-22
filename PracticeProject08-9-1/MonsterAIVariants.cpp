#include "stdafx.h"
#include "MonsterAIVariants.h"

#include "GameScene.h"
#include "Grid.h"
#include "Object.h"
#include "AnimatorComponent.h"
#include "MonsterAnimController.h"
#include "HealthComponent.h"

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

	m_bossPostMeleeTurnDuration = 0.25f;
	m_bossPostMeleeTurnRemaining = 0.0f;
	m_bossPostMeleeTurnSpeedDegrees = 900.0f;

	m_bossSpellTurnSpeedDegrees = 720.0f;
}

bool CBossAIComponent::AcquireTarget()
{
	CGameScene* scene = GetScene();
	if ( !scene )
		return false;

	if ( !scene->IsLocalMonsterChaseEnabled() )
		return false;

	CGameObject* player = scene->GetPlayer();
	if ( !player )
		return false;

	if ( auto* hp = player->GetComponent<CHealthComponent>() )
	{
		if ( hp->IsDead() )
			return false;
	}

	if ( !IsPlayerInsideBossBattleZone(player) )
		return false;

	SetTarget(player);
	return true;
}

void CBossAIComponent::UpdateBehavior(float dt)
{
	UpdateBossCooldowns(dt);

	if ( !HasValidTarget() )
	{
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
		BeginReturnHome();
		return;
	}

	const float distanceToTarget = GetDistanceToTargetXZ();

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
		m_bossPostMeleeTurnRemaining = m_bossPostMeleeTurnDuration;
	}

	if ( IsBossSpellActionPlaying() )
	{
		ClearPath();

		// 원거리 공격 중에는 이동하지 않고, 플레이어 방향으로만 부드럽게 회전한다.
		SmoothFaceTowardsTarget(
			target,
			dt,
			m_bossSpellTurnSpeedDegrees
		);

		return;
	}

	if ( UpdateBossPostMeleeTurn(dt) )
	{
		return;
	}

	const bool canStartAction = CanStartBossAction();

	if ( canStartAction )
	{
		if ( distanceToTarget <= m_bossMeleeRange )
		{
			ClearPath();
			SetMonsterLocomotionState(EMonsterAnimState::Idle);

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

		FaceTowards(target->GetPosition());
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

	// 근거리 공격 직후 자연 회전 중에도 이동하지 않는다.
	if ( m_bossPostMeleeTurnRemaining > 0.0f )
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

bool CBossAIComponent::UpdateBossPostMeleeTurn(float dt)
{
	if ( m_bossPostMeleeTurnRemaining <= 0.0f )
		return false;

	CGameObject* target = GetTarget();
	if ( !target )
	{
		m_bossPostMeleeTurnRemaining = 0.0f;
		return false;
	}

	ClearPath();
	SetMonsterLocomotionState(EMonsterAnimState::Idle);

	SmoothFaceTowardsTarget(
		target,
		dt,
		m_bossPostMeleeTurnSpeedDegrees
	);

	if ( dt > 0.0f )
	{
		m_bossPostMeleeTurnRemaining -= dt;
		if ( m_bossPostMeleeTurnRemaining < 0.0f )
			m_bossPostMeleeTurnRemaining = 0.0f;
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
	}

	return IsWorldPositionInsideMegaGridCenterZone(
		player->GetPosition(),
		5
	);
}

bool CBossAIComponent::CanStartBossAction() const
{
	if ( IsAIActionLockedByAnimation() )
		return false;

	if ( m_bossGlobalActionCooldownRemaining > 0.0f )
		return false;

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
	return TryPerformBossCommand(EMonsterAnimCommand::Attack);
}

bool CBossAIComponent::TryPerformSpellAttack()
{
	m_pendingAttackIntent = EBossAttackIntent::Spell;
	return TryPerformBossCommand(EMonsterAnimCommand::Spell);
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