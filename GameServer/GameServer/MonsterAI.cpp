#include "pch.h"
#include "MonsterAI.h"

#include "Enemy.h"
#include "Player.h"
#include "Room.h"
#include "NavMesh.h"

CMonsterAI::CMonsterAI(OwnerT* owner)
	: CComponentT(owner)
{
}

void CMonsterAI::OnUpdate(float dt)
{
	if (!GetOwner()) return;

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

	m_repathTimer -= dt;

	// 직선 이동 모드 (spawner pool Ghoul)
	if (m_useDirectMove)
	{
		if (m_initialAdvanceDist > 0.f)
		{
			const float step = m_moveSpeed * dt;
			const auto pos = GetOwner()->GetPosition();
			const GameMath::Vec3 next(
				pos.x + m_initialAdvanceDir.x * step,
				pos.y,
				pos.z + m_initialAdvanceDir.z * step);
			FaceTowards(next);
			GetOwner()->SetPosition(next);
			GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_RUN);
			GetOwner()->SetLastMoveDir(m_initialAdvanceDir);
			m_initialAdvanceDist = std::max(0.f, m_initialAdvanceDist - step);
			return;
		}

		if (!AcquireTarget())
		{
			m_pTarget = nullptr;
			GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_IDLE);
			return;
		}

		const auto myPos = GetOwner()->GetPosition();
		const auto targetPos = m_pTarget->GetPosition();
		const float distSq = DistSqXZ(myPos, targetPos);

		if (distSq <= m_attackRange * m_attackRange)
		{
			FaceTowards(targetPos);
			if (m_attackCooldownRemaining <= 0.f)
			{
				GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_ATTACK);
				GetOwner()->SetAnimTick(GRoom->GetAnimClockTick());
				m_attackCooldownRemaining = m_attackCooldownSec;
			}
			return;
		}

		MoveDirectTowards(targetPos, m_moveSpeed * dt);
		return;
	}

	// 일반 NavMesh 모드
	if (!AcquireTarget())
	{
		m_pTarget = nullptr;
		m_currentPath.clear();
		m_trianglePath.clear();
		m_currentPathIndex = 0;
		GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_IDLE);
		return;
	}

	const auto myPos = GetOwner()->GetPosition();
	const auto targetPos = m_pTarget->GetPosition();
	const float distSq = DistSqXZ(myPos, targetPos);

	if (distSq <= m_attackRange * m_attackRange)
	{
		m_currentPath.clear();
		m_trianglePath.clear();
		m_currentPathIndex = 0;
		FaceTowards(targetPos);

		if (m_attackCooldownRemaining <= 0.f)
		{
			GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_ATTACK);
			GetOwner()->SetAnimTick(GRoom->GetAnimClockTick());
			m_attackCooldownRemaining = m_attackCooldownSec;
		}
		return;
	}

	bool repathChanged = false;
	if (m_repathTimer <= 0.f || m_currentPath.empty() || m_currentPathIndex >= m_currentPath.size())
		repathChanged = RebuildPathToTarget();

	const bool followingPath = FollowCurrentPath(dt);
	if (!followingPath)
	{
		FaceTowards(targetPos);
		GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_IDLE);
	}
}

bool CMonsterAI::AcquireTarget()
{
	if (!GRoom) return false;

	CServerObject* nearest = nullptr;
	float bestSq = FLT_MAX;
	const auto myPos = GetOwner()->GetPosition();

	const float innerZoneSq = (m_innerZoneRadius > 0.f) ? (m_innerZoneRadius * m_innerZoneRadius) : -1.f;

	for (const auto& [id, player] : GRoom->GetPlayers())
	{
		if (!player) continue;
		if (player->IsDead()) continue;

		if (innerZoneSq >= 0.f)
		{
			if (DistSqXZ(m_innerZoneCenter, player->GetPosition()) > innerZoneSq)
				continue;
		}

		const float dSq = DistSqXZ(myPos, player->GetPosition());
		if (dSq < bestSq)
		{
			bestSq = dSq;
			nearest = player.get();
		}
	}

	const bool hadTarget = (m_pTarget != nullptr);
	m_pTarget = nearest;

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
	if (!SampleNavMeshPosition(m_pTarget->GetPosition(), goalPos))   return false;

	std::vector<int> newTrianglePath;
	std::vector<GameMath::Vec3> newPath;

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

bool CMonsterAI::MoveTowards(const GameMath::Vec3& goal, float maxStep)
{
	const auto pos = GetOwner()->GetPosition();
	GameMath::Vec3 d(goal.x - pos.x, 0.f, goal.z - pos.z);
	const float len = d.LengthXZ();
	if (len <= 1e-6f) return false;

	const float step = (maxStep < len) ? maxStep : len;
	const GameMath::Vec3 dir(d.x / len, 0.f, d.z / len);
	FaceTowards(goal);

	GameMath::Vec3 next(pos.x + dir.x * step, pos.y, pos.z + dir.z * step);
	GameMath::Vec3 sampled{};
	if (SampleNavMeshPosition(next, sampled))
		GetOwner()->SetPosition(sampled);
	else
		GetOwner()->SetPosition(next);

	GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_RUN);
	GetOwner()->SetLastMoveDir(dir);
	return true;
}

bool CMonsterAI::FollowCurrentPath(float dt)
{
	if (m_currentPath.empty()) return false;
	if (m_currentPathIndex >= m_currentPath.size()) return false;

	const float moveDistance = m_moveSpeed * dt;
	if (moveDistance <= 0.f) return false;

	while (m_currentPathIndex < m_currentPath.size())
	{
		const auto& wp = m_currentPath[m_currentPathIndex];
		if (DistSqXZ(GetOwner()->GetPosition(), wp) <=
			m_pathPointReachDistance * m_pathPointReachDistance)
		{
			++m_currentPathIndex;
			continue;
		}
		return MoveTowards(wp, moveDistance);
	}
	return false;
}

bool CMonsterAI::MoveDirectTowards(const GameMath::Vec3& goal, float maxStep)
{
	const auto pos = GetOwner()->GetPosition();
	GameMath::Vec3 d(goal.x - pos.x, 0.f, goal.z - pos.z);
	const float len = d.LengthXZ();
	if (len <= 1e-6f) return false;

	const float step = (maxStep < len) ? maxStep : len;
	const GameMath::Vec3 dir(d.x / len, 0.f, d.z / len);
	FaceTowards(goal);
	GetOwner()->SetPosition(GameMath::Vec3(pos.x + dir.x * step, pos.y, pos.z + dir.z * step));
	GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_RUN);
	GetOwner()->SetLastMoveDir(dir);
	return true;
}

void CMonsterAI::SetDirectMoveMode(float advanceDist, const GameMath::Vec3& homeDir, float innerZoneRadius, const GameMath::Vec3& zoneCenter)
{
	m_useDirectMove = true;
	m_initialAdvanceDist = advanceDist;
	m_initialAdvanceDir = homeDir;
	m_innerZoneRadius = innerZoneRadius;
	m_innerZoneCenter = zoneCenter;
	m_hasNotifiedFirstChase = false;
	m_pTarget = nullptr;
	m_currentPath.clear();
	m_trianglePath.clear();
	m_currentPathIndex = 0;
}

void CMonsterAI::ConfigureFromWeapon(Protocol::WeaponType weaponType)
{
	switch (weaponType)
	{
	case Protocol::WEAPON_TYPE_BOW:
	case Protocol::WEAPON_TYPE_CANON:
		m_attackRange = 15.0f;
		m_meleeArcDeg = 360.0f;
		break;
	case Protocol::WEAPON_TYPE_SWORD:
		m_attackRange = 2.0f;
		m_meleeArcDeg = 90.0f;
		break;
	case Protocol::WEAPON_TYPE_AXE:
		m_attackRange = 2.5f;
		m_meleeArcDeg = 90.0f;
		break;
	default:
		m_attackRange = 2.0f;
		m_meleeArcDeg = 180.0f;
		break;
	}
}