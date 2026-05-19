//-----------------------------------------------------------------------------
// File: MonsterAIComponent.h
//-----------------------------------------------------------------------------

#pragma once

#include "Component.h"
#include "MonsterAnimTypes.h"

#include <vector>
#include <string>

class CGameScene;
class CNavMesh;
class CAnimatorComponent;
class CMonsterAnimController;
class CAnimController;
class CGameObject;

class CMonsterAIComponent : public CComponentT<CMonsterAIComponent>
{
public:
	explicit CMonsterAIComponent(CGameObject* owner);
	virtual ~CMonsterAIComponent() = default;

public:
	void OnCreate(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd) override;
	void OnUpdate(float dt) override;

public:
	void SetScene(CGameScene* scene) { m_pScene = scene; }
	CGameScene* GetScene() const { return m_pScene; }

	void SetTarget(CGameObject* target);
	CGameObject* GetTarget() const { return m_pTarget; }

	void ClearTarget();

public:
	void SetEnabledAI(bool enabled) { m_bAIEnabled = enabled; }
	bool IsAIEnabled() const { return m_bAIEnabled; }

	void SetDetectRange(float v) { SetChaseStopRange(v); }

	void SetChaseStartRange(float v)
	{
		m_chaseStartRange = ( v > 0.0f ) ? v : 0.0f;
	}

	void SetChaseStopRange(float v)
	{
		m_chaseStopRange = ( v > 0.0f ) ? v : 0.0f;
	}

	void SetChaseRanges(float startRange, float stopRange)
	{
		SetChaseStartRange(startRange);
		SetChaseStopRange(stopRange);
	}

	void SetAttackRange(float v) { m_attackRange = v; }
	void SetMoveSpeed(float v) { m_moveSpeed = v; }
	void SetAttackCooldown(float v) { m_attackCooldown = v; }
	void SetRepathInterval(float v) { m_repathInterval = v; }
	void SetPathPointReachDistance(float v) { m_pathPointReachDistance = v; }
	void SetGoalReachDistance(float v) { m_goalReachDistance = v; }
	void SetFacingYawOffsetDegrees(float v) { m_facingYawOffsetDegrees = v; }

	float GetDetectRange() const { return m_chaseStopRange; }
	float GetChaseStartRange() const { return m_chaseStartRange; }
	float GetChaseStopRange() const { return m_chaseStopRange; }

	float GetAttackRange() const { return m_attackRange; }
	float GetMoveSpeed() const { return m_moveSpeed; }
	float GetAttackCooldown() const { return m_attackCooldown; }
	float GetRepathInterval() const { return m_repathInterval; }
	float GetPathPointReachDistance() const { return m_pathPointReachDistance; }
	float GetGoalReachDistance() const { return m_goalReachDistance; }
	float GetFacingYawOffsetDegrees() const { return m_facingYawOffsetDegrees; }

public:
	bool HasValidTarget() const;
	bool HasPath() const { return !m_currentPath.empty() && ( m_currentPathIndex < m_currentPath.size() ); }

	float GetDistanceToTargetXZ() const;
	float GetDistanceToPointXZ(const XMFLOAT3& p) const;

	bool IsTargetInDetectRange() const;

	bool IsObjectInChaseStartRange(CGameObject* obj) const;
	bool IsObjectInChaseStopRange(CGameObject* obj) const;

	bool IsTargetInChaseStartRange() const;
	bool IsTargetInChaseStopRange() const;

	bool IsTargetInAttackRange() const;

	bool CanAttackNow() const;
	void ConsumeAttackCooldown();

public:
	bool RebuildPathToTarget();
	void ClearPath();

protected:
	// 파생 클래스에서 override
	virtual bool AcquireTarget();
	virtual void UpdateBehavior(float dt);
	virtual bool TryPerformAttack() = 0;

	// 이동/공격 정책
	virtual bool ShouldMoveTowardsTarget() const;
	virtual bool ShouldRepath() const;
	virtual bool CanMoveNow() const;
	virtual bool CanThinkNow() const;
	virtual bool CanAttackNowByState() const;

	virtual bool CanStartAttackAgainstTarget() const;

	bool IsAIActionLockedByAnimation() const;

	bool EnsureMovementBoundsInitialized();
	XMFLOAT3 ClampPointToMovementBounds(const XMFLOAT3& p) const;
	XMFLOAT3 GetTargetMoveGoalPosition() const;

protected:
	CNavMesh* GetNavMesh() const;
	CAnimatorComponent* GetAnimatorComponent() const;
	CAnimController* GetAnimController() const;
	CMonsterAnimController* GetMonsterAnimController() const;
	void SetMonsterLocomotionState(EMonsterAnimState state);

	XMFLOAT3 GetOwnerPosition() const;
	bool FaceTowards(const XMFLOAT3& targetPos);
	bool MoveTowards(const XMFLOAT3& targetPos, float maxStepDistance);

	bool TryMoveDirectlyToTarget(float dt);
	bool FollowCurrentPath(float dt);

	bool HasDirectNavMeshLineTo(
		const XMFLOAT3& targetPos,
		bool clampTargetToMovementBounds = true) const;
	bool SampleNavMeshPosition(const XMFLOAT3& inPos, XMFLOAT3& outPos, int* outTri = nullptr) const;

	void UpdateCooldowns(float dt);
	void UpdatePathTimers(float dt);

protected:
	CGameScene* m_pScene = nullptr;
	CGameObject* m_pTarget = nullptr;

	bool m_bAIEnabled = true;

	float m_chaseStartRange = 12.0f;
	float m_chaseStopRange = 12.0f;

	float m_attackRange = 1.5f;
	float m_moveSpeed = 2.0f;
	float m_attackCooldown = 1.0f;
	float m_repathInterval = 0.35f;

	float m_pathPointReachDistance = 0.15f;
	float m_goalReachDistance = 0.75f;
	float m_facingYawOffsetDegrees = 0.0f;

	float m_attackCooldownRemaining = 0.0f;
	float m_postAttackMoveLockRemaining = 0.0f;
	float m_postAttackMoveLockDuration = 0.25f;

	float m_repathTimer = 0.0f;
	bool m_bMovementBoundsInitialized = false;

	float m_movementMinX = 0.0f;
	float m_movementMaxX = 0.0f;
	float m_movementMinZ = 0.0f;
	float m_movementMaxZ = 0.0f;

	std::vector<int> m_trianglePath;
	std::vector<XMFLOAT3> m_currentPath;
	size_t m_currentPathIndex = 0;
};