#include "pch.h"
#include "MonsterAI.h"

#include "Enemy.h"
#include "Player.h"
#include "Room.h"
#include "NavMesh.h"

namespace
{
	bool ShouldPrintTrackLog(const CServerObject* owner)
	{
		return owner &&
			(owner->GetAnimState() != Protocol::ANIMATION_TYPE_IDLE);
	}
}

CMonsterAI::CMonsterAI(OwnerT* owner)
	: CComponentT(owner)
{
}

void CMonsterAI::OnUpdate(float dt)
{
	if (!GetOwner())
		return;

	// ★ HIT/DIE 등 피격 상태일 때는 AI 행동 중단
	const auto animState = GetOwner()->GetAnimState();
	if (animState == Protocol::ANIMATION_TYPE_HIT ||
		animState == Protocol::ANIMATION_TYPE_DIE)
		return;

	auto PrintState = [&](const char* state, bool repathChanged, bool followingPath)
		{
			if (!ShouldPrintTrackLog(GetOwner()))
				return;

			const bool hasPath = (!m_currentPath.empty() && m_currentPathIndex < m_currentPath.size());
			cout
				<< "[MONSTER AI] id=" << GetOwner()->GetObjectId()
				<< " state=" << state
				<< " hasPath=" << (hasPath ? 1 : 0)
				<< " following=" << (followingPath ? 1 : 0)
				<< " repathChanged=" << (repathChanged ? 1 : 0)
				<< endl;
		};

	if (m_attackCooldownRemaining > 0.f)
	{
		m_attackCooldownRemaining -= dt;
		if (m_attackCooldownRemaining < 0.f)
			m_attackCooldownRemaining = 0.f;
	}

	m_repathTimer -= dt;

	if (!AcquireTarget())
	{
		m_pTarget = nullptr;
		m_currentPath.clear();
		m_trianglePath.clear();
		m_currentPathIndex = 0;
		GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_IDLE);
		PrintState("NO_TARGET", false, false);
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
			GetOwner()->SetAnimTick(GRoom->GetTick());
			m_attackCooldownRemaining = m_attackCooldownSec;
			// ★ 여기서 즉시 히트하지 않음 → TickAdvance에서 arc 판정
		}
		else
		{
			GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_IDLE);
		}
		return;
	}

	if (distSq <= m_attackRange * m_attackRange)
	{
		m_currentPath.clear();
		m_trianglePath.clear();
		m_currentPathIndex = 0;
		FaceTowards(targetPos);

		if (m_attackCooldownRemaining <= 0.f)
		{
			GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_ATTACK);
			m_pTarget->SetAnimState(Protocol::ANIMATION_TYPE_HIT);
			m_pTarget->SetAnimTick(GRoom->GetTick());
			m_attackCooldownRemaining = m_attackCooldownSec;
		}
		else
		{
			GetOwner()->SetAnimState(Protocol::ANIMATION_TYPE_IDLE);
		}

		//PrintState("ATTACK_RANGE", false, false);
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
		//PrintState("IDLE_NO_FOLLOW", repathChanged, false);
	}
	else
	{
		//PrintState(repathChanged ? "FOLLOWING_REPATHED" : "FOLLOWING_PATH", repathChanged, true);
	}
}

bool CMonsterAI::AcquireTarget()
{
	if (!GRoom)
		return false;

	CServerObject* nearest = nullptr;
	float bestSq = FLT_MAX;
	const auto myPos = GetOwner()->GetPosition();

	for (const auto& [id, player] : GRoom->GetPlayers())
	{
		if (!player)
			continue;

		const float dSq = DistSqXZ(myPos, player->GetPosition());
		if (dSq < bestSq)
		{
			bestSq = dSq;
			nearest = player.get();
		}
	}

	m_pTarget = nearest;
	return m_pTarget != nullptr;
}

const CNavMesh* CMonsterAI::GetNavMesh() const
{
	return GRoom ? GRoom->GetNavMesh() : nullptr;
}

bool CMonsterAI::SampleNavMeshPosition(const GameMath::Vec3& in, GameMath::Vec3& out) const
{
	const CNavMesh* nav = GetNavMesh();
	if (!nav || !nav->IsLoaded())
		return false;

	return nav->SamplePosition(in, out, nullptr);
}

bool CMonsterAI::RebuildPathToTarget()
{
	m_repathTimer = m_repathInterval;
	if (!m_pTarget)
		return false;
	const CNavMesh* nav = GetNavMesh();
	if (!nav || !nav->IsLoaded())
		return false;

	GameMath::Vec3 startPos{};
	GameMath::Vec3 goalPos{};
	if (!SampleNavMeshPosition(GetOwner()->GetPosition(), startPos))
		return false;
	if (!SampleNavMeshPosition(m_pTarget->GetPosition(), goalPos))
		return false;

	// 임시 버퍼에 먼저 받기
	std::vector<int> newTrianglePath;
	std::vector<GameMath::Vec3> newPath;

	if (!nav->FindPath(startPos, goalPos, newTrianglePath, newPath))
	{
		return false;
	}

	// 성공하면 그때 교체
	m_trianglePath = std::move(newTrianglePath);
	m_currentPath = std::move(newPath);
	m_currentPathIndex = 0;

	while (m_currentPathIndex < m_currentPath.size())
	{
		if (DistSqXZ(GetOwner()->GetPosition(), m_currentPath[m_currentPathIndex]) > m_pathPointReachDistance * m_pathPointReachDistance)
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
	if (dx * dx + dz * dz <= 1e-8f)
		return;

	const float yawDeg = atan2f(dx, dz) * GameMath::RAD_TO_DEG;
	if (auto* tr = GetOwner()->GetComponent<CCommonTransformComponent>())
		tr->SetYawDegrees(yawDeg);
}

bool CMonsterAI::MoveTowards(const GameMath::Vec3& goal, float maxStep)
{
	const auto pos = GetOwner()->GetPosition();
	GameMath::Vec3 d(goal.x - pos.x, 0.f, goal.z - pos.z);
	const float len = d.LengthXZ();
	if (len <= 1e-6f)
		return false;

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
	return true;
}

bool CMonsterAI::FollowCurrentPath(float dt)
{
	if (m_currentPath.empty()) 
		return false;
	if (m_currentPathIndex >= m_currentPath.size())
		return false;

	const float moveDistance = m_moveSpeed * dt;
	if (moveDistance <= 0.f)
		return false;

	while (m_currentPathIndex < m_currentPath.size())
	{
		const auto& wp = m_currentPath[m_currentPathIndex];
		if (DistSqXZ(GetOwner()->GetPosition(), wp) <= m_pathPointReachDistance * m_pathPointReachDistance)
		{
			++m_currentPathIndex;
			continue;
		}

		return MoveTowards(wp, moveDistance);
	}

	return false;
}

void CMonsterAI::ConfigureFromWeapon(Protocol::WeaponType weaponType)
{
	switch (weaponType)
	{
	case Protocol::WEAPON_TYPE_BOW:
	case Protocol::WEAPON_TYPE_CANON:
		m_attackRange = 15.0f;     // 원거리
		m_meleeArcDeg = 360.0f;    // 원거리는 방향 무관
		break;
	case Protocol::WEAPON_TYPE_SWORD:
		m_attackRange = 2.0f;
		m_meleeArcDeg = 90.0f;     // 사분원
		break;
	case Protocol::WEAPON_TYPE_AXE:
		m_attackRange = 2.5f;
		m_meleeArcDeg = 90.0f;     // 사분원
		break;
	default: // 무기 없는 잡몹, 보스
		m_attackRange = 2.0f;
		m_meleeArcDeg = 180.0f;    // 반원
		break;
	}
}