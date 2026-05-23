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
	m_bBossHitReactionPolicyConfigured = false;

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

void CBossAIComponent::UpdateBehavior(float dt)
{
	ConfigureBossHitReactionPolicy();

	UpdateBossCooldowns(dt);

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

			char buf[256];
			sprintf_s(
				buf,
				"[BossAI][OpeningSpell] confirmed spell phase. boss=%p\n",
				static_cast< void* >( GetOwner() )
			);
			OutputDebugStringA(buf);
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
				char buf[512];
				sprintf_s(
					buf,
					"[BossAI][OpeningSpell] waiting after request. boss=%p age=%.3f dist=%.3f canStart=%d locked=%d meleePlaying=%d spellPlaying=%d\n",
					static_cast< void* >( GetOwner() ),
					m_bossOpeningSpellRequestAgeSec,
					distanceToTarget,
					canStartAction ? 1 : 0,
					IsAIActionLockedByAnimation() ? 1 : 0,
					IsBossMeleeActionPlaying() ? 1 : 0,
					IsBossSpellActionPlaying() ? 1 : 0
				);
				OutputDebugStringA(buf);

				// 0.5초 동안 SpellPhase로 안 들어갔고 현재 action lock도 없다면,
				// 이전 요청은 컨트롤러에서 소비되지 않은 것으로 보고 다시 시도하게 한다.
				if ( !IsAIActionLockedByAnimation() && !IsBossSpellActionPlaying() )
				{
					OutputDebugStringA(
						"[BossAI][OpeningSpell] retry spell request next frame.\n"
					);

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
				char buf[512];
				sprintf_s(
					buf,
					"[BossAI][OpeningSpell] blocked before request. boss=%p dist=%.3f globalCd=%.3f spellCd=%.3f locked=%d\n",
					static_cast< void* >( GetOwner() ),
					distanceToTarget,
					m_bossGlobalActionCooldownRemaining,
					m_bossSpellCooldownRemaining,
					IsAIActionLockedByAnimation() ? 1 : 0
				);
				OutputDebugStringA(buf);

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

			char buf[512];
			sprintf_s(
				buf,
				"[BossAI][OpeningSpell] request spell. boss=%p dist=%.3f globalCd=%.3f spellCd=%.3f lockedAfter=%d\n",
				static_cast< void* >( GetOwner() ),
				distanceToTarget,
				m_bossGlobalActionCooldownRemaining,
				m_bossSpellCooldownRemaining,
				IsAIActionLockedByAnimation() ? 1 : 0
			);
			OutputDebugStringA(buf);
		}
		else
		{
			char buf[512];
			sprintf_s(
				buf,
				"[BossAI][OpeningSpell] request failed. boss=%p dist=%.3f globalCd=%.3f spellCd=%.3f locked=%d\n",
				static_cast< void* >( GetOwner() ),
				distanceToTarget,
				m_bossGlobalActionCooldownRemaining,
				m_bossSpellCooldownRemaining,
				IsAIActionLockedByAnimation() ? 1 : 0
			);
			OutputDebugStringA(buf);
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

	char buf[512];
	sprintf_s(
		buf,
		"[BossAI][PostMeleeEvade] begin pos=(%.3f, %.3f, %.3f) target=(%.3f, %.3f, %.3f) dir=(%.3f, %.3f, %.3f) dist=%.3f\n",
		pos.x,
		pos.y,
		pos.z,
		target.x,
		target.y,
		target.z,
		m_bossPostMeleeEvadeDirection.x,
		m_bossPostMeleeEvadeDirection.y,
		m_bossPostMeleeEvadeDirection.z,
		dist
	);
	OutputDebugStringA(buf);
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

		OutputDebugStringA("[BossAI][PostMeleeEvade] end.\n");
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
	CaptureBossMeleeAttackForward();

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