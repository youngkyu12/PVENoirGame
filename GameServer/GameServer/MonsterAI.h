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

private:
	bool AcquireTarget();
	bool RebuildPathToTarget();
	bool FollowCurrentPath(float dt);
	bool MoveTowards(const GameMath::Vec3& goal, float maxStep);
	void FaceTowards(const GameMath::Vec3& goal);
	bool SampleNavMeshPosition(const GameMath::Vec3& in, GameMath::Vec3& out) const;
	const CNavMesh* GetNavMesh() const;
	void ConfigureFromWeapon(Protocol::WeaponType weaponType);

private:
	CServerObject* m_pTarget = nullptr;
	float m_detectRange = 99999.0f;
	float m_attackRange = 1.5f;
	float m_meleeArcDeg = 360.0f;     // 근접 공격 부채꼴 각도
	float m_moveSpeed = 2.0f;
	float m_attackCooldownSec = 1.0f;
	float m_repathInterval = 0.15f;
	float m_pathPointReachDistance = 0.10f;
	float m_goalReachDistance = 0.25f;

	float m_attackCooldownRemaining = 0.f;
	float m_repathTimer = 0.f;
	float m_debugPrintTimer = 0.f;
	std::vector<int> m_trianglePath;
	std::vector<GameMath::Vec3> m_currentPath;
	size_t m_currentPathIndex = 0;
};
