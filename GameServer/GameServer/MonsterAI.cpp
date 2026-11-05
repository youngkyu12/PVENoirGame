#include "pch.h"
#include "MonsterAI.h"

#include "Enemy.h"
#include "Player.h"
#include "Room.h"
#include "NavMesh.h"
#include "MonsterAITrace.h"
#include "MonsterAIPerformance.h"

#include <algorithm>
#include <cmath>

namespace
{
	uint32 HashCrowdEnemyId(uint64 id)
	{
		id ^= id >> 33;
		id *= 0xff51afd7ed558ccdULL;
		id ^= id >> 33;
		id *= 0xc4ceb9fe1a85ec53ULL;
		id ^= id >> 33;
		return static_cast<uint32>(id ^ (id >> 32));
	}

	float CrowdHash01(uint32 hash)
	{
		return static_cast<float>(hash & 0x00FFFFFFu) / 16777216.0f;
	}

	bool NormalizeCrowdDirection(GameMath::Vec3& direction)
	{
		const float length = direction.LengthXZ();
		if (length <= 1.0e-6f) return false;
		direction.x /= length;
		direction.y = 0.0f;
		direction.z /= length;
		return true;
	}
}

CMonsterAI::CMonsterAI(OwnerT* owner)
	: CComponentT(owner)
	, m_profile(&GetMonsterAIProfile(EMonsterAIProfile::FieldIdle))
{
	assert(ValidateMonsterAIProfiles());
}

void CMonsterAI::OnUpdate(float dt)
{
	if (!GetOwner()) return;
	MONSTER_AI_TRACE_FRAME(*this);

	if (GetOwner()->IsDead()) return;

	const auto animState = GetOwner()->GetAnimState();

	if (animState == Protocol::ANIMATION_TYPE_HIT ||
		animState == Protocol::ANIMATION_TYPE_DIE)
		return;

	if (animState == Protocol::ANIMATION_TYPE_ATTACK)
	{
		constexpr int kAttackDurationTicks = 20;
		const int elapsed = static_cast<int>(GRoom->GetAnimClockTick()) - GetOwner()->GetAnimTick();
		if (elapsed <= kAttackDurationTicks)
			return;

		GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_IDLE);
	}

	if (m_attackCooldownRemaining > 0.f)
	{
		m_attackCooldownRemaining -= dt;
		if (m_attackCooldownRemaining < 0.f)
			m_attackCooldownRemaining = 0.f;
	}

	if (m_postAttackMoveLockRemaining > 0.f)
	{
		m_postAttackMoveLockRemaining -= dt;
		if (m_postAttackMoveLockRemaining < 0.f)
			m_postAttackMoveLockRemaining = 0.f;
	}

	m_repathTimer -= dt;

	if (m_profile->lifecyclePolicy->UpdateBeforeTargeting(*this, dt))
		return;

	const bool wasChasing = m_isChasing;
	if (!AcquireTarget())
	{
		m_pTarget = nullptr;
		ClearChasePath();
		m_profile->lifecyclePolicy->UpdateNoTarget(*this, dt, wasChasing);
		return;
	}

	m_bReturningHome = false;

	const auto myPos = GetOwner()->GetPosition();
	const auto targetPos = m_pTarget->GetPosition();
	const float distSq = DistSqXZ(myPos, targetPos);

	if (distSq <= m_attackRange * m_attackRange)
	{
		ClearChasePath();
		m_state = EMonsterAIState::Attack;
		FaceTowards(targetPos);

		if (m_attackCooldownRemaining <= 0.f)
		{
			GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_ATTACK);
			GetOwner()->SetAnimTick(GRoom->GetAnimClockTick());
			MONSTER_AI_TRACE(*this, m_state, EMonsterAITraceEvent::AttackStarted, true);
			m_attackCooldownRemaining = m_attackCooldownSec;
			m_postAttackMoveLockRemaining = m_postAttackMoveLockDuration;

			if (static_cast<CEnemy*>(GetOwner())->GetWeaponState() == Protocol::WEAPON_TYPE_BOW)
			{
				constexpr float  kEnemyArrowSpeed     = 14.0f;
				constexpr uint32 kEnemyArrowLifeTicks = 375;
				GRoom->FireEnemyArrow(GetOwner(), kEnemyArrowSpeed, kEnemyArrowLifeTicks);
			}
		}
		return;
	}

	m_profile->chasePolicy->UpdateChase(*this, dt);
}

bool CMonsterAI::AcquireTarget()
{
	MONSTER_AI_PERF_INCREMENT(EMonsterAIPerformanceCounter::AcquireTarget);
	return m_profile && m_profile->targetPolicy && m_profile->targetPolicy->FindTarget(*this);
}

bool CMonsterAI::FindBossRoomTarget()
{
	if (!GRoom) return false;

	CServerObject* nearest = nullptr;
	float bestSq = FLT_MAX;
	const auto myPos = GetOwner()->GetPosition();

	for (const auto& [id, player] : GRoom->GetPlayers())
	{
		if (!player) continue;
		if (player->IsDead()) continue;
		if (!GRoom->IsBossRoomChaseTarget(id)) continue;

		const float dSq = DistSqXZ(myPos, player->GetPosition());
		if (dSq < bestSq)
		{
			bestSq = dSq;
			nearest = player.get();
		}
	}

	const bool hadTarget = (m_pTarget != nullptr);
	m_pTarget = nearest;
	m_isChasing = (m_pTarget != nullptr);
	if (!hadTarget && m_pTarget)
		MONSTER_AI_TRACE(*this, m_state, EMonsterAITraceEvent::TargetAcquired, true);
	else if (hadTarget && !m_pTarget)
		MONSTER_AI_TRACE(*this, m_state, EMonsterAITraceEvent::TargetLost, true);

	if (!hadTarget && m_pTarget != nullptr && !m_hasNotifiedFirstChase)
	{
		m_hasNotifiedFirstChase = true;
		GRoom->OnMonsterFirstChase(GetOwner()->GetObjectId());
	}

	return m_pTarget != nullptr;
}

bool CMonsterAI::FindRangeConeTarget(bool useInnerZone)
{
	if (!GRoom) return false;

	CServerObject* nearest = nullptr;
	float bestSq = FLT_MAX;
	const auto myPos = GetOwner()->GetPosition();
	const float innerZoneSq = useInnerZone && m_innerZoneRadius > 0.f
		? m_innerZoneRadius * m_innerZoneRadius
		: -1.f;

	for (const auto& [id, player] : GRoom->GetPlayers())
	{
		if (!player) continue;
		if (player->IsDead()) continue;

		if (innerZoneSq >= 0.f)
		{
			if (DistSqXZ(m_innerZoneCenter, player->GetPosition()) > innerZoneSq)
				continue;
		}
		else if (!CanChaseTargetByMegaGrid(player.get()))
		{
			continue;
		}

		const float dSq = DistSqXZ(myPos, player->GetPosition());
		if (dSq < bestSq)
		{
			bestSq = dSq;
			nearest = player.get();
		}
	}

	// two-threshold chase detection
	if (nearest != nullptr)
	{
		if (!m_isChasing)
		{
			if (bestSq > m_chaseStartRange * m_chaseStartRange ||
				(!IsObjectInChaseStartCone(nearest) && !IsPlayerRunning(nearest)))
				nearest = nullptr;
			else
				m_isChasing = true;
		}
		else
		{
			if (bestSq > m_chaseStopRange * m_chaseStopRange ||
				!CanChaseTargetByMegaGrid(nearest))
			{
				m_isChasing = false;
				nearest = nullptr;
			}
		}
	}
	else
	{
		m_isChasing = false;
	}

	const bool hadTarget = (m_pTarget != nullptr);
	m_pTarget = nearest;
	if (!hadTarget && m_pTarget)
		MONSTER_AI_TRACE(*this, m_state, EMonsterAITraceEvent::TargetAcquired, true);
	else if (hadTarget && !m_pTarget)
		MONSTER_AI_TRACE(*this, m_state, EMonsterAITraceEvent::TargetLost, true);

	if (!hadTarget && m_pTarget != nullptr && !m_hasNotifiedFirstChase)
	{
		m_hasNotifiedFirstChase = true;
		GRoom->OnMonsterFirstChase(GetOwner()->GetObjectId());
	}

	return m_pTarget != nullptr;
}

const CNavMesh* CMonsterAI::GetNavMesh() const
{
	return GRoom ? GRoom->GetNavMesh() : nullptr;
}

bool CMonsterAI::SampleNavMeshPosition(const GameMath::Vec3& in, GameMath::Vec3& out) const
{
	const CNavMesh* nav = GetNavMesh();
	if (!nav || !nav->IsLoaded()) return false;
	return nav->SamplePosition(in, out, nullptr);
}

bool CMonsterAI::RebuildPathToTarget()
{
	m_repathTimer = m_repathInterval;
	if (!m_pTarget) return false;

	const CNavMesh* nav = GetNavMesh();
	if (!nav || !nav->IsLoaded()) return false;

	GameMath::Vec3 startPos{};
	GameMath::Vec3 goalPos{};
	if (!SampleNavMeshPosition(GetOwner()->GetPosition(), startPos)) return false;
	if (!SampleNavMeshPosition(GetTargetMoveGoalPosition(), goalPos)) return false;
	goalPos = ClampPointToMovementBounds(goalPos);

	std::vector<int> newTrianglePath;
	std::vector<GameMath::Vec3> newPath;

	MONSTER_AI_PERF_INCREMENT(EMonsterAIPerformanceCounter::FindPath);
	if (!nav->FindPath(startPos, goalPos, newTrianglePath, newPath))
		return false;

	m_trianglePath = std::move(newTrianglePath);
	m_currentPath = std::move(newPath);
	m_currentPathIndex = 0;

	while (m_currentPathIndex < m_currentPath.size())
	{
		if (DistSqXZ(GetOwner()->GetPosition(), m_currentPath[m_currentPathIndex]) >
			m_pathPointReachDistance * m_pathPointReachDistance)
			break;
		++m_currentPathIndex;
	}
	return m_currentPathIndex < m_currentPath.size();
}

void CMonsterAI::FaceTowards(const GameMath::Vec3& goal)
{
	const auto pos = GetOwner()->GetPosition();
	const float dx = goal.x - pos.x;
	const float dz = goal.z - pos.z;
	if (dx * dx + dz * dz <= 1e-8f) return;

	const float yawDeg = atan2f(dx, dz) * GameMath::RAD_TO_DEG;
	if (auto* tr = GetOwner()->GetComponent<CCommonTransformComponent>())
		tr->SetYawDegrees(yawDeg);
}

bool CMonsterAI::RotateOwnerYawTowards(float targetYawDeg, float maxStepDeg)
{
	if (!GetOwner())
		return false;

	const float currentYaw = GetOwner()->GetYaw();
	const float targetYaw = GameMath::NormalizeYaw(targetYawDeg);
	float deltaYaw = targetYaw - currentYaw;
	if (deltaYaw > 180.0f)
		deltaYaw -= 360.0f;
	else if (deltaYaw < -180.0f)
		deltaYaw += 360.0f;

	const float absDelta = fabsf(deltaYaw);
	if (absDelta <= maxStepDeg)
	{
		GetOwner()->SetYaw(targetYaw);
		return true;
	}

	const float step = (deltaYaw > 0.0f) ? maxStepDeg : -maxStepDeg;
	GetOwner()->SetYaw(currentYaw + step);
	return false;
}

bool CMonsterAI::MoveTowards(const GameMath::Vec3& goal, float maxStep, bool clampToMovementBounds)
{
	const auto pos = GetOwner()->GetPosition();
	const GameMath::Vec3 moveGoal = clampToMovementBounds ? ClampPointToMovementBounds(goal) : goal;
	GameMath::Vec3 d(moveGoal.x - pos.x, 0.f, moveGoal.z - pos.z);
	const float len = d.LengthXZ();
	if (len <= 1e-6f) return false;

	const float step = (maxStep < len) ? maxStep : len;
	const GameMath::Vec3 dir(d.x / len, 0.f, d.z / len);
	FaceTowards(moveGoal);

	GameMath::Vec3 next(pos.x + dir.x * step, pos.y, pos.z + dir.z * step);
	if (clampToMovementBounds)
		next = ClampPointToMovementBounds(next);

	GameMath::Vec3 sampled{};
	if (SampleNavMeshPosition(next, sampled))
		GetOwner()->SetPosition(clampToMovementBounds ? ClampPointToMovementBounds(sampled) : sampled);
	else
		GetOwner()->SetPosition(next);

	GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_RUN);
	GetOwner()->SetLastMoveDir(dir);
	return true;
}

bool CMonsterAI::FollowCurrentPath(float dt)
{
	MONSTER_AI_PERF_INCREMENT(EMonsterAIPerformanceCounter::FollowPath);
	if (m_currentPath.empty()) return false;
	if (m_currentPathIndex >= m_currentPath.size()) return false;

	const float moveDistance = m_moveSpeed * dt;
	if (moveDistance <= 0.f) return false;

	while (m_currentPathIndex < m_currentPath.size())
	{
		const auto wp = ClampPointToMovementBounds(m_currentPath[m_currentPathIndex]);
		if (DistSqXZ(GetOwner()->GetPosition(), wp) <=
			m_pathPointReachDistance * m_pathPointReachDistance)
		{
			++m_currentPathIndex;
			continue;
		}

		// waypoint가 목표 반대 방향이면 경로를 버리고 다음 틱에 재탐색
		if (m_pTarget != nullptr)
		{
			const auto myPos    = GetOwner()->GetPosition();
			const auto goalPos  = GetTargetMoveGoalPosition();
			const float toWpX   = wp.x - myPos.x;
			const float toWpZ   = wp.z - myPos.z;
			const float toGoalX = goalPos.x - myPos.x;
			const float toGoalZ = goalPos.z - myPos.z;
			if (toWpX * toGoalX + toWpZ * toGoalZ < 0.f)
			{
				m_currentPath.clear();
				m_trianglePath.clear();
				m_currentPathIndex = 0;
				m_repathTimer = 0.f;
				return false;
			}
		}

		return MoveTowards(wp, moveDistance);
	}
	return false;
}

bool CMonsterAI::ComputeCrowdChaseDirection(
	const GameMath::Vec3& goal,
	GameMath::Vec3& direction,
	float& goalDistance,
	float& speedScale) const
{
	const auto pos = GetOwner()->GetPosition();
	direction = GameMath::Vec3(goal.x - pos.x, 0.0f, goal.z - pos.z);
	const float targetDistance = direction.LengthXZ();
	goalDistance = targetDistance;
	speedScale = 1.0f;
	if (!NormalizeCrowdDirection(direction)) return false;

	// Only the two no-NavMesh chase profiles use crowd steering.
	const auto profile = GetProfileId();
	if (profile != EMonsterAIProfile::SpawnerRush && profile != EMonsterAIProfile::BossRoomPersistent)
		return true;

	float slotRadiusMin = 2.0f;
	float slotRadiusMax = 6.0f;
	float slotStartDistance = 36.0f;
	float slotFullDistance = 20.0f;
	float separationRadius = 3.0f;
	float separationWeight = 1.15f;
	if (profile == EMonsterAIProfile::SpawnerRush)
	{
		slotStartDistance = 32.0f;
		slotFullDistance = 18.0f;
		separationWeight = 1.20f;
	}
	else
	{
		const auto* enemy = static_cast<const CEnemy*>(GetOwner());
		switch (enemy->type)
		{
		case Protocol::ENEMY_TYPE_WARRIOR:
			slotRadiusMin = 3.0f;
			slotRadiusMax = 8.0f;
			slotStartDistance = 42.0f;
			slotFullDistance = 24.0f;
			separationRadius = 3.5f;
			separationWeight = 1.20f;
			break;
		case Protocol::ENEMY_TYPE_ARCHER:
			slotRadiusMin = 14.0f;
			slotRadiusMax = 24.0f;
			slotStartDistance = 70.0f;
			slotFullDistance = 45.0f;
			separationRadius = 4.0f;
			separationWeight = 1.05f;
			break;
		case Protocol::ENEMY_TYPE_MUTANT:
			slotRadiusMin = 4.0f;
			slotRadiusMax = 9.0f;
			slotStartDistance = 48.0f;
			slotFullDistance = 28.0f;
			separationRadius = 4.5f;
			separationWeight = 1.30f;
			break;
		default:
			break;
		}
	}

	// Match the local distance blend: straight at range, spread on approach,
	// then fade steering near attack range so melee enemies can reach the target.
	const float farBlend = std::clamp(
		(slotStartDistance - targetDistance) / std::max(0.001f, slotStartDistance - slotFullDistance), 0.0f, 1.0f);
	const float nearBlend = std::clamp((targetDistance - m_attackRange - 2.0f) / 6.0f, 0.0f, 1.0f);
	const float blend = (std::min)(farBlend, nearBlend);
	const float slotBlend = blend * blend * (3.0f - 2.0f * blend);

	const uint32 hash = HashCrowdEnemyId(GetOwner()->GetObjectId());
	speedScale = 0.96f + CrowdHash01(hash ^ 0x9E3779B9u) * 0.08f;
	GameMath::Vec3 chaseGoal = goal;
	if (slotBlend > 1.0e-4f)
	{
		const float angle = CrowdHash01(hash) * 6.28318530718f;
		const float radius = slotRadiusMin + (slotRadiusMax - slotRadiusMin) *
			CrowdHash01(hash * 1664525u + 1013904223u);
		chaseGoal.x += std::cos(angle) * radius * slotBlend;
		chaseGoal.z += std::sin(angle) * radius * slotBlend;
	}

	GameMath::Vec3 seek(chaseGoal.x - pos.x, 0.0f, chaseGoal.z - pos.z);
	goalDistance = seek.LengthXZ();
	if (!NormalizeCrowdDirection(seek)) seek = direction;
	direction = seek;
	if (GRoom && slotBlend > 0.0f)
	{
		const auto separation = GRoom->GetEnemyCrowdSeparation(GetOwner()->GetObjectId(), pos, separationRadius);
		direction.x += separation.x * separationWeight * slotBlend;
		direction.z += separation.z * separationWeight * slotBlend;
	}
	if (!NormalizeCrowdDirection(direction)) direction = seek;
	return true;
}

bool CMonsterAI::MoveDirectTowards(const GameMath::Vec3& goal, float maxStep)
{
	MONSTER_AI_PERF_INCREMENT(EMonsterAIPerformanceCounter::DirectMove);
	if (maxStep <= 0.0f) return false;

	GameMath::Vec3 direction{};
	float goalDistance = 0.0f;
	float speedScale = 1.0f;
	if (!ComputeCrowdChaseDirection(goal, direction, goalDistance, speedScale)) return false;
	const float step = (std::min)(maxStep * speedScale, goalDistance);
	if (step <= 0.0f) return false;

	const auto pos = GetOwner()->GetPosition();
	const GameMath::Vec3 next(pos.x + direction.x * step, pos.y, pos.z + direction.z * step);
	FaceTowards(GameMath::Vec3(pos.x + direction.x, pos.y, pos.z + direction.z));
	GetOwner()->SetPosition(next);
	GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_RUN);
	GetOwner()->SetLastMoveDir(direction);
	return true;
}

void CMonsterAI::SetHomePosition(const GameMath::Vec3& pos)
{
	m_homePosition = pos;
	m_homeYawDeg = GetOwner() ? GetOwner()->GetYaw() : 0.f;
	m_homeMegaGridNumber = GRoom ? GRoom->GetMegaGridNumberFromWorldPosition(pos) : 0;
	m_bMovementBoundsInitialized = false;

	if (GRoom && m_homeMegaGridNumber > 0)
	{
		m_bMovementBoundsInitialized = GRoom->GetMegaGridCenterMovementBounds(
			m_homeMegaGridNumber,
			m_movementMinX,
			m_movementMaxX,
			m_movementMinZ,
			m_movementMaxZ);
	}

	ResetPatrolState();
}

void CMonsterAI::SetChaseRanges(float startRange, float stopRange)
{
	m_chaseStartRange = startRange;
	m_chaseStopRange  = stopRange;
	m_isChasing       = false;
}

void CMonsterAI::ApplyProfile(EMonsterAIProfile profile, const MonsterAIProfileTransition& transition)
{
	m_profile = &GetMonsterAIProfile(profile);
	m_initialAdvanceDist = transition.initialAdvanceDistance;
	m_initialAdvanceDir = transition.initialAdvanceDirection;
	m_innerZoneRadius = transition.innerZoneRadius;
	m_innerZoneCenter = transition.innerZoneCenter;
	m_hasNotifiedFirstChase = false;
	m_isChasing = false;
	m_pTarget = nullptr;
	ClearChasePath();
	m_bReturningHome = false;
	ClearReturnPath();
	m_bPatrolEnabled = false;
	ResetPatrolState();
	m_state = EMonsterAIState::Idle;

	if (transition.beginReturnHome)
		BeginReturnHome();
}

void CMonsterAI::SetPatrolEnabled(bool enabled)
{
	m_bPatrolEnabled = enabled;
	ResetPatrolState();

	const EMonsterAIProfile current = m_profile->id;
	if (current == EMonsterAIProfile::FieldIdle || current == EMonsterAIProfile::FieldPatrol)
	{
		m_profile = &GetMonsterAIProfile(enabled
			? EMonsterAIProfile::FieldPatrol
			: EMonsterAIProfile::FieldIdle);
	}
}

void CMonsterAI::ConfigureFromWeapon(Protocol::WeaponType weaponType)
{
	switch (weaponType)
	{
	case Protocol::WEAPON_TYPE_BOW:
	case Protocol::WEAPON_TYPE_CANON:
		m_attackRange     = 15.0f;
		m_meleeArcDeg     = 360.0f;
		m_chaseStartRange = 50.0f;   // BowMan
		m_chaseStopRange  = 50.0f;
		break;
	case Protocol::WEAPON_TYPE_SWORD:
		m_attackRange     = 2.0f;
		m_meleeArcDeg     = 90.0f;
		m_chaseStartRange = 35.0f;   // SwordMan
		m_chaseStopRange  = 50.0f;
		break;
	case Protocol::WEAPON_TYPE_AXE:
		m_attackRange     = 2.5f;
		m_meleeArcDeg     = 90.0f;
		m_chaseStartRange = 10.0f;   // Ghoul
		m_chaseStopRange  = 50.0f;
		break;
	default:
		m_attackRange     = 2.0f;
		m_meleeArcDeg     = 180.0f;
		m_chaseStartRange = 25.0f;   // Mutant
		m_chaseStopRange  = 50.0f;
		break;
	}
}

void CMonsterAI::ResetToHome()
{
	m_pTarget         = nullptr;
	m_isChasing       = false;
	m_bReturningHome  = false;
	m_currentPath.clear();
	m_trianglePath.clear();
	m_currentPathIndex = 0;
	m_returnPath.clear();
	m_returnTrianglePath.clear();
	m_returnPathIndex  = 0;
	m_postAttackMoveLockRemaining = 0.f;
	ResetPatrolState();
	GetOwner()->SetPosition(m_homePosition);
	GetOwner()->SetYaw(m_homeYawDeg);
	m_state = EMonsterAIState::Idle;
}

bool CMonsterAI::IsAtHome() const
{
	constexpr float kHomeTolerance = 0.2f;
	return GameMath::DistSqXZ(GetOwner()->GetPosition(), m_homePosition) <= kHomeTolerance * kHomeTolerance;
}

bool CMonsterAI::BeginReturnHome()
{
	const bool wasReturningHome = m_bReturningHome;
	const EMonsterAIState previousState = m_state;
	m_bReturningHome = true;
	m_state = EMonsterAIState::ReturnHome;
	m_pTarget = nullptr;
	ResetPatrolState();
	ClearChasePath();
	ClearReturnPath();
	if (!wasReturningHome)
		MONSTER_AI_TRACE(*this, previousState, EMonsterAITraceEvent::ReturnStarted, true);

	if (IsAtHome())
	{
		m_bReturningHome = false;
		m_state = EMonsterAIState::Idle;
		MONSTER_AI_TRACE(*this, EMonsterAIState::ReturnHome, EMonsterAITraceEvent::ReturnCompleted, true);
		return true;
	}

	const CNavMesh* nav = GetNavMesh();
	if (!nav || !nav->IsLoaded()) return false;

	GameMath::Vec3 startPos{}, goalPos{};
	if (!SampleNavMeshPosition(GetOwner()->GetPosition(), startPos)) return false;
	if (!SampleNavMeshPosition(m_homePosition, goalPos))             return false;

	MONSTER_AI_PERF_INCREMENT(EMonsterAIPerformanceCounter::FindPath);
	if (!nav->FindPath(startPos, goalPos, m_returnTrianglePath, m_returnPath))
		return false;

	m_returnPathIndex = 0;
	while (m_returnPathIndex < m_returnPath.size())
	{
		if (DistSqXZ(GetOwner()->GetPosition(), m_returnPath[m_returnPathIndex]) >
			m_pathPointReachDistance * m_pathPointReachDistance)
			break;
		++m_returnPathIndex;
	}
	return true;
}

bool CMonsterAI::UpdateReturnHome(float dt)
{
	if (IsAtHome())
	{
		m_bReturningHome = false;
		ClearReturnPath();
		m_state = EMonsterAIState::Idle;
		MONSTER_AI_TRACE(*this, EMonsterAIState::ReturnHome, EMonsterAITraceEvent::ReturnCompleted, true);
		GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_IDLE);
		return true;
	}

	GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_WALK);

	if (m_returnPath.empty() || m_returnPathIndex >= m_returnPath.size())
	{
		BeginReturnHome();
		return false;
	}

	const float moveDistance = m_moveSpeed * dt;

	while (m_returnPathIndex < m_returnPath.size())
	{
		const auto& wp = m_returnPath[m_returnPathIndex];
		if (DistSqXZ(GetOwner()->GetPosition(), wp) <= m_pathPointReachDistance * m_pathPointReachDistance)
		{
			++m_returnPathIndex;
			continue;
		}

		MoveTowards(wp, moveDistance, false);
		GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_WALK);
		return false;
	}

	ResetToHome();
	MONSTER_AI_TRACE(*this, EMonsterAIState::ReturnHome, EMonsterAITraceEvent::ReturnCompleted, true);
	GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_IDLE);
	return false;
}

bool CMonsterAI::HasDirectNavMeshLineTo(const GameMath::Vec3& target) const
{
	MONSTER_AI_PERF_INCREMENT(EMonsterAIPerformanceCounter::NavMeshLineOfSight);
	const CNavMesh* nav = GetNavMesh();
	if (!nav || !nav->IsLoaded()) return false;
	return nav->HasLineOfSight(GetOwner()->GetPosition(), target);
}

bool CMonsterAI::CanMoveNow() const
{
	return m_postAttackMoveLockRemaining <= 0.f;
}

bool CMonsterAI::IsPlayerRunning(const CServerObject* player) const
{
	return player && player->GetAnimState() == Protocol::ANIMATION_TYPE_RUN;
}

bool CMonsterAI::IsObjectInChaseStartCone(const CServerObject* obj) const
{
	if (!obj || !GetOwner())
		return false;

	const auto ownerPos = GetOwner()->GetPosition();
	const auto targetPos = obj->GetPosition();
	GameMath::Vec3 toTarget(targetPos.x - ownerPos.x, 0.f, targetPos.z - ownerPos.z);
	const float len = toTarget.LengthXZ();
	if (len <= 1e-6f)
		return true;

	toTarget.x /= len;
	toTarget.z /= len;

	const auto forward = GetOwner()->GetLook().Normalized();
	const float dot = forward.x * toTarget.x + forward.z * toTarget.z;
	constexpr float kCosHalfAngle60 = 0.5f;
	return dot >= kCosHalfAngle60;
}

bool CMonsterAI::CanChaseTargetByMegaGrid(const CServerObject* target) const
{
	if (!target || !GRoom)
		return false;

	return GRoom->IsPositionInsideMegaGridApproachZone(target->GetPosition());
}

GameMath::Vec3 CMonsterAI::ClampPointToMovementBounds(const GameMath::Vec3& p) const
{
	if (!m_bMovementBoundsInitialized)
		return p;

	return GameMath::Vec3(
		std::clamp(p.x, m_movementMinX, m_movementMaxX),
		p.y,
		std::clamp(p.z, m_movementMinZ, m_movementMaxZ));
}

GameMath::Vec3 CMonsterAI::GetTargetMoveGoalPosition() const
{
	if (!m_pTarget)
		return GetOwner() ? GetOwner()->GetPosition() : GameMath::Vec3::Zero();

	return ClampPointToMovementBounds(m_pTarget->GetPosition());
}

bool CMonsterAI::IsOutsideHomeMegaGrid() const
{
	if (!GRoom || m_homeMegaGridNumber <= 0 || !GetOwner())
		return false;

	return !GRoom->IsPositionInsideMegaGridNumber(GetOwner()->GetPosition(), m_homeMegaGridNumber);
}

bool CMonsterAI::IsAtHomeForAwakeRemoval() const
{
	return IsAtHome();
}

void CMonsterAI::ResetPatrolState()
{
	m_bPatrolInitialized = false;
	m_bPatrolTurning = false;
	m_patrolTargetSign = 1;
	m_patrolTurnTargetYawDeg = 0.0f;
}

void CMonsterAI::ClearChasePath()
{
	m_currentPath.clear();
	m_trianglePath.clear();
	m_currentPathIndex = 0;
}

void CMonsterAI::ClearReturnPath()
{
	m_returnPath.clear();
	m_returnTrianglePath.clear();
	m_returnPathIndex = 0;
}

void CMonsterAI::SetIdleState()
{
	m_state = EMonsterAIState::Idle;
	GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_IDLE);
}

GameMath::Vec3 CMonsterAI::GetPatrolEndpoint(int targetSign) const
{
	const int sign = (targetSign < 0) ? -1 : 1;
	const GameMath::Vec3 forward = GameMath::YawToLook(m_homeYawDeg);
	return GameMath::Vec3(
		m_homePosition.x + forward.x * m_patrolHalfDistance * static_cast<float>(sign),
		m_homePosition.y,
		m_homePosition.z + forward.z * m_patrolHalfDistance * static_cast<float>(sign));
}

float CMonsterAI::GetPatrolFacingYawDegreesForTargetSign(int targetSign) const
{
	const int sign = (targetSign < 0) ? -1 : 1;
	const GameMath::Vec3 forward = GameMath::YawToLook(m_homeYawDeg);
	return GameMath::NormalizeYaw(
		atan2f(forward.x * static_cast<float>(sign), forward.z * static_cast<float>(sign)) *
		GameMath::RAD_TO_DEG);
}

bool CMonsterAI::UpdateIdlePatrol(float dt)
{
	if (!m_bPatrolEnabled || m_bReturningHome || !GetOwner())
		return false;

	if (!m_bPatrolInitialized)
	{
		m_bPatrolInitialized = true;
		m_bPatrolTurning = false;
		m_patrolTargetSign = 1;
		m_patrolTurnTargetYawDeg = GetPatrolFacingYawDegreesForTargetSign(m_patrolTargetSign);
	}

	m_state = EMonsterAIState::Patrol;

	if (dt <= 0.f || !CanMoveNow())
	{
		GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_IDLE);
		return true;
	}

	if (m_bPatrolTurning)
	{
		GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_WALK);
		if (RotateOwnerYawTowards(m_patrolTurnTargetYawDeg, m_patrolTurnSpeedDegrees * dt))
			m_bPatrolTurning = false;
		return true;
	}

	const GameMath::Vec3 endpoint = GetPatrolEndpoint(m_patrolTargetSign);
	if (DistSqXZ(GetOwner()->GetPosition(), endpoint) <=
		m_patrolEndpointReachDistance * m_patrolEndpointReachDistance)
	{
		m_patrolTargetSign = -m_patrolTargetSign;
		MONSTER_AI_TRACE(*this, m_state, EMonsterAITraceEvent::PatrolDirectionChanged, true);
		m_patrolTurnTargetYawDeg = GetPatrolFacingYawDegreesForTargetSign(m_patrolTargetSign);
		m_bPatrolTurning = true;
		GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_WALK);
		RotateOwnerYawTowards(m_patrolTurnTargetYawDeg, m_patrolTurnSpeedDegrees * dt);
		return true;
	}

	const float moveDistance = m_walkMoveSpeed * dt;
	if (moveDistance <= 0.f)
		return true;

	MoveTowards(endpoint, moveDistance, false);
	GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_WALK);
	return true;
}
