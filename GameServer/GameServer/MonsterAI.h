#pragma once

#include "BaseComponent.h"
#include "GameMath.h"

class CNavMesh;
class CServerObject;

class CMonsterAI : public CComponentT<CMonsterAI>
{
public:
	explicit CMonsterAI(OwnerT* owner);

	void OnUpdate(float dt) override;
	void SetDirectMoveMode(float advanceDist, const GameMath::Vec3& homeDir, float innerZoneRadius, const GameMath::Vec3& zoneCenter);
	void SetChaseRanges(float startRange, float stopRange);
	void SetAttackRange(float range) { m_attackRange = range; }
	void SetMoveSpeed(float speed)   { m_moveSpeed   = speed; }

private:
	bool AcquireTarget();
	bool RebuildPathToTarget();
	bool FollowCurrentPath(float dt);
	bool MoveTowards(const GameMath::Vec3& goal, float maxStep);
	bool MoveDirectTowards(const GameMath::Vec3& goal, float maxStep);
	void FaceTowards(const GameMath::Vec3& goal);
	bool SampleNavMeshPosition(const GameMath::Vec3& in, GameMath::Vec3& out) const;
	const CNavMesh* GetNavMesh() const;
	void ConfigureFromWeapon(Protocol::WeaponType weaponType);

private:
	CServerObject* m_pTarget = nullptr;
	float m_detectRange = 99999.0f;
	float m_attackRange = 1.5f;
	float m_meleeArcDeg = 360.0f;     // ���� ���� ��ä�� ����
	float m_moveSpeed = 2.0f;
	float m_attackCooldownSec = 1.0f;
	float m_repathInterval = 0.15f;
	float m_pathPointReachDistance = 0.10f;
	float m_goalReachDistance = 0.25f;

	float m_chaseStartRange = 99999.0f;
	float m_chaseStopRange  = 99999.0f;
	bool  m_isChasing       = false;

	float m_attackCooldownRemaining = 0.f;
	float m_repathTimer = 0.f;
	float m_debugPrintTimer = 0.f;
	std::vector<int> m_trianglePath;
	std::vector<GameMath::Vec3> m_currentPath;
	size_t m_currentPathIndex = 0;

	// spawner pool
	bool m_useDirectMove = false;
	float m_initialAdvanceDist = 0.f;
	GameMath::Vec3 m_initialAdvanceDir{};
	float m_innerZoneRadius = 0.f;
	GameMath::Vec3 m_innerZoneCenter{};
	bool m_hasNotifiedFirstChase = false;
};
