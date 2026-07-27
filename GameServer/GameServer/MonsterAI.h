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
	void SetInfiniteDirectChaseMode();
	void ClearInfiniteDirectChaseMode();
	void SetChaseRanges(float startRange, float stopRange);
	void SetAttackRange(float range) { m_attackRange = range; }
	void SetMoveSpeed(float speed)   { m_moveSpeed   = speed; }
	void SetWalkMoveSpeed(float speed) { m_walkMoveSpeed = (speed > 0.f) ? speed : 0.f; }
	void SetHomePosition(const GameMath::Vec3& pos);
	void SetPatrolEnabled(bool enabled) { m_bPatrolEnabled = enabled; ResetPatrolState(); }
	void ResetToHome();
	bool IsOutsideHomeMegaGrid() const;
	bool IsAtHomeForAwakeRemoval() const;

private:
	bool AcquireTarget();
	bool RebuildPathToTarget();
	bool FollowCurrentPath(float dt);
	bool MoveTowards(const GameMath::Vec3& goal, float maxStep, bool clampToMovementBounds = true);
	bool MoveDirectTowards(const GameMath::Vec3& goal, float maxStep);
	void FaceTowards(const GameMath::Vec3& goal);
	bool SampleNavMeshPosition(const GameMath::Vec3& in, GameMath::Vec3& out) const;
	const CNavMesh* GetNavMesh() const;
	void ConfigureFromWeapon(Protocol::WeaponType weaponType);

	bool IsAtHome() const;
	bool BeginReturnHome();
	bool UpdateReturnHome(float dt);
	bool HasDirectNavMeshLineTo(const GameMath::Vec3& target) const;
	bool CanMoveNow() const;
	bool IsPlayerRunning(const CServerObject* player) const;
	bool IsObjectInChaseStartCone(const CServerObject* obj) const;
	bool CanChaseTargetByMegaGrid(const CServerObject* target) const;
	GameMath::Vec3 ClampPointToMovementBounds(const GameMath::Vec3& p) const;
	GameMath::Vec3 GetTargetMoveGoalPosition() const;
	bool UpdateIdlePatrol(float dt);
	void ResetPatrolState();
	GameMath::Vec3 GetPatrolEndpoint(int targetSign) const;
	float GetPatrolFacingYawDegreesForTargetSign(int targetSign) const;
	bool RotateOwnerYawTowards(float targetYawDeg, float maxStepDeg);

private:
	CServerObject* m_pTarget = nullptr;
	float m_detectRange = 99999.0f;
	float m_attackRange = 1.5f;
	float m_meleeArcDeg = 360.0f;     // ���� ���� ��ä�� ����
	float m_moveSpeed = 2.0f;
	float m_walkMoveSpeed = 1.0f;
	float m_attackCooldownSec = 1.0f;
	float m_postAttackMoveLockDuration = 0.25f;
	float m_repathInterval = 0.15f;
	float m_pathPointReachDistance = 0.10f;
	float m_goalReachDistance = 0.25f;

	float m_chaseStartRange = 99999.0f;
	float m_chaseStopRange  = 99999.0f;
	bool  m_isChasing       = false;

	float m_attackCooldownRemaining = 0.f;
	float m_postAttackMoveLockRemaining = 0.f;
	float m_repathTimer = 0.f;
	float m_debugPrintTimer = 0.f;
	std::vector<int> m_trianglePath;
	std::vector<GameMath::Vec3> m_currentPath;
	size_t m_currentPathIndex = 0;

	// return-to-home
	GameMath::Vec3 m_homePosition{};
	float          m_homeYawDeg = 0.f;
	int            m_homeMegaGridNumber = 0;
	bool           m_bMovementBoundsInitialized = false;
	float          m_movementMinX = 0.f;
	float          m_movementMaxX = 0.f;
	float          m_movementMinZ = 0.f;
	float          m_movementMaxZ = 0.f;
	bool           m_bReturningHome  = false;
	std::vector<int>             m_returnTrianglePath;
	std::vector<GameMath::Vec3>  m_returnPath;
	size_t         m_returnPathIndex = 0;

	// patrol
	bool m_bPatrolEnabled = false;
	bool m_bPatrolInitialized = false;
	bool m_bPatrolTurning = false;
	int m_patrolTargetSign = 1;
	float m_patrolTurnTargetYawDeg = 0.0f;
	float m_patrolHalfDistance = 5.0f;
	float m_patrolEndpointReachDistance = 0.15f;
	float m_patrolTurnSpeedDegrees = 240.0f;

	// spawner pool
	bool m_useDirectMove = false;
	bool m_useInfiniteDirectChase = false;
	float m_initialAdvanceDist = 0.f;
	GameMath::Vec3 m_initialAdvanceDir{};
	float m_innerZoneRadius = 0.f;
	GameMath::Vec3 m_innerZoneCenter{};
	bool m_hasNotifiedFirstChase = false;
};
