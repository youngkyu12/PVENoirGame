//-----------------------------------------------------------------------------
// File: MonsterAIVariants.h
//-----------------------------------------------------------------------------

#pragma once

#include "MonsterAIComponent.h"

//-----------------------------------------------------------------------------
// Ghoul
//-----------------------------------------------------------------------------
class CGhoulAIComponent : public CMonsterAIComponent
{
public:
	explicit CGhoulAIComponent(CGameObject* owner);
	~CGhoulAIComponent() override = default;

public:
	TypeId GetTypeId() const override
	{
		return CComponent::StaticTypeId<CGhoulAIComponent>();
	}

protected:
	bool TryPerformAttack() override;
};

//-----------------------------------------------------------------------------
// Enemy Spawner Ghoul
// 6 / 8번 메가그리드 스포너 전용.
//-----------------------------------------------------------------------------
class CEnemySpawnerGhoulAIComponent final : public CGhoulAIComponent
{
public:
	explicit CEnemySpawnerGhoulAIComponent(CGameObject* owner);
	~CEnemySpawnerGhoulAIComponent() override = default;

public:
	TypeId GetTypeId() const override
	{
		return CComponent::StaticTypeId<CEnemySpawnerGhoulAIComponent>();
	}

	void OnUpdate(float dt) override;

	bool ForceChaseTarget(CGameObject* target) override;

	void ConfigureSpawnerGhoulAI(
		int megaGridNumber,
		float initialAdvanceDistance = 60.0f
	);

protected:
	bool AcquireTarget() override;
	void UpdateBehavior(float dt) override;

	EMonsterAnimState GetChaseLocomotionState() const override;
	EMonsterAnimState GetWalkLocomotionState() const override;

private:
	bool UpdateInitialAdvance(float dt);

	bool IsPlayerValidSpawnerTarget(CGameObject* player) const;
	bool IsWorldPositionInsideInnerEmptyZone(const XMFLOAT3& pos) const;
	bool GetInnerEmptyZoneCenter(XMFLOAT3& outCenter) const;

	CGameObject* FindNearestPlayerInsideInnerEmptyZone() const;

	bool MoveDirectNoNavTowards(const XMFLOAT3& targetPos, float maxStepDistance);
	bool MoveDirectNoNavByDirection(const XMFLOAT3& direction, float maxStepDistance);

private:
	int m_spawnerMegaGridNumber = -1;

	bool m_bInitialAdvanceActive = true;
	float m_initialAdvanceRemainingDistance = 60.0f;

	static constexpr float kInnerEmptyZoneHalfExtent = 50.0f;
};

//-----------------------------------------------------------------------------
// Boss Stage Monster
// 5번 메가그리드 전용.
// - target 획득 조건: 플레이어가 5번 보스 스테이지에 있는가
// - chase 이동: navmesh 없이 target 방향 직진
// - target 없음: 기존 return-home 로직 사용
// - SwordMan / BowMan patrol 전후 이동 없음
//-----------------------------------------------------------------------------
class CBossStageMonsterAIComponent final : public CMonsterAIComponent
{
public:
	enum class EKind : uint8_t
	{
		Ghoul = 0,
		SwordMan,
		BowMan,
		Mutant
	};

public:
	explicit CBossStageMonsterAIComponent(CGameObject* owner);
	~CBossStageMonsterAIComponent() override = default;

public:
	TypeId GetTypeId() const override
	{
		return CComponent::StaticTypeId<CBossStageMonsterAIComponent>();
	}

	void OnUpdate(float dt) override;

	bool ForceChaseTarget(CGameObject* target) override;

	void ConfigureBossStageMonsterAI(EKind kind);

protected:
	bool AcquireTarget() override;
	void UpdateBehavior(float dt) override;

	bool CanStartAttackAgainstTarget() const override;
	bool TryPerformAttack() override;

	EMonsterAnimState GetChaseLocomotionState() const override;
	EMonsterAnimState GetWalkLocomotionState() const override;

private:
	bool IsPlayerValidBossStageTarget(CGameObject* player) const;
	bool HasAnyValidPlayerInsideBossStage() const;
	CGameObject* FindNearestPlayerInsideBossStage() const;

	bool MoveDirectNoNavTowards(const XMFLOAT3& targetPos, float maxStepDistance);
	bool MoveDirectNoNavByDirection(const XMFLOAT3& direction, float maxStepDistance);

private:
	EKind m_kind = EKind::Ghoul;
};

//-----------------------------------------------------------------------------
// SwordMan
//-----------------------------------------------------------------------------
class CSwordManAIComponent final : public CMonsterAIComponent
{
public:
	explicit CSwordManAIComponent(CGameObject* owner);
	~CSwordManAIComponent() override = default;

public:
	TypeId GetTypeId() const override
	{
		return CComponent::StaticTypeId<CSwordManAIComponent>();
	}

protected:
	bool TryPerformAttack() override;
};

//-----------------------------------------------------------------------------
// BowMan
//-----------------------------------------------------------------------------
class CBowManAIComponent final : public CMonsterAIComponent
{
public:
	explicit CBowManAIComponent(CGameObject* owner);
	~CBowManAIComponent() override = default;

public:
	TypeId GetTypeId() const override
	{
		return CComponent::StaticTypeId<CBowManAIComponent>();
	}

protected:
	bool CanStartAttackAgainstTarget() const override;
	bool TryPerformAttack() override;
};

//-----------------------------------------------------------------------------
// Mutant
//-----------------------------------------------------------------------------
class CMutantAIComponent final : public CMonsterAIComponent
{
public:
	explicit CMutantAIComponent(CGameObject* owner);
	~CMutantAIComponent() override = default;

public:
	TypeId GetTypeId() const override
	{
		return CComponent::StaticTypeId<CMutantAIComponent>();
	}

protected:
	bool TryPerformAttack() override;
};

//-----------------------------------------------------------------------------
// Boss
//-----------------------------------------------------------------------------
class CBossAIComponent final : public CMonsterAIComponent
{
public:
	explicit CBossAIComponent(CGameObject* owner);
	~CBossAIComponent() override = default;

public:
	TypeId GetTypeId() const override
	{
		return CComponent::StaticTypeId<CBossAIComponent>();
	}

	void ResetBossCallState();

protected:
	bool AcquireTarget() override;
	void UpdateBehavior(float dt) override;
	bool TryPerformAttack() override;

	bool CanMoveNow() const override;
	bool CanThinkNow() const override;
	bool CanRotateNow() const override;

	EMonsterAnimState GetChaseLocomotionState() const override;
	EMonsterAnimState GetWalkLocomotionState() const override;

private:
	enum class EBossAttackIntent : uint8_t
	{
		Melee = 0,
		Spell,
		Call
	};

private:
	void UpdateBossCooldowns(float dt);
	void ConfigureBossHitReactionPolicy();

	void UpdateBossCallThresholdState();
	bool HasPendingBossCall() const;
	bool TryRequestPendingBossCall(CGameObject* target, float dt);

	bool IsPlayerInsideBossBattleZone(CGameObject* player) const;
	bool CanStartBossAction() const;

	bool TryPerformBossCommand(EMonsterAnimCommand command);
	bool TryPerformMeleeAttack();
	bool TryPerformSpellAttack();
	bool TryPerformCall();

	void ConsumeBossMeleeCooldown();
	void ConsumeBossSpellCooldown();
	void ConsumeBossCallCooldown();

	bool IsBossMeleeActionPlaying() const;
	bool IsBossSpellActionPlaying() const;
	bool IsBossCallActionPlaying() const;

	bool SmoothFaceTowardsTarget(
		CGameObject* target,
		float dt,
		float turnSpeedDegreesPerSec
	);

	void CaptureBossMeleeAttackForward();
	void BeginBossPostMeleeEvade();
	bool UpdateBossPostMeleeEvade(float dt);

	bool SelectBossPostMeleeEvadeDirection(XMFLOAT3& outDir) const;
	bool IsBossPostMeleeEvadeDestinationValid(
		const XMFLOAT3& from,
		const XMFLOAT3& dir
	) const;

	XMFLOAT3 ClampBossPostMeleeEvadePointToStage(const XMFLOAT3& p) const;

private:
	EBossAttackIntent m_pendingAttackIntent = EBossAttackIntent::Melee;

	float m_bossMeleeRange = 7.0f;
	float m_bossPreferredSpellRange = 12.0f;

	float m_bossGlobalActionCooldown = 0.8f;
	float m_bossMeleeCooldown = 2.0f;
	float m_bossSpellCooldown = 3.5f;

	float m_bossGlobalActionCooldownRemaining = 0.0f;
	float m_bossMeleeCooldownRemaining = 0.0f;
	float m_bossSpellCooldownRemaining = 0.0f;

	bool m_bBossWasMeleeActionPlaying = false;

	static constexpr float kBossHitReactionAnimSuperArmorSec = 1.0f;
	bool m_bBossHitReactionPolicyConfigured = false;

	// HP 75%, 50%, 25% 진입 시 각각 1회씩 Call 예약.
	// bit 0 = 3/4 이하, bit 1 = 2/4 이하, bit 2 = 1/4 이하.
	uint8_t m_bossCallThresholdMask = 0;
	int m_bossPendingCallCount = 0;

	bool m_bBossCallCommandRequested = false;
	bool m_bBossCallConsumePendingOnStart = false;
	float m_bossCallRequestAgeSec = 0.0f;
	float m_bossCallTurnSpeedDegrees = 720.0f;

	bool m_bBossOpeningSpellPending = true;
	bool m_bBossOpeningSpellRequested = false;
	float m_bossOpeningSpellRequestAgeSec = 0.0f;

	// 근거리 공격 후 즉시 거리를 벌리는 회피 이동.
	// 보스는 공중 몬스터이므로 별도 이동 애니메이션 없이 Idle 유지.
	static constexpr float kBossPostMeleeEvadeDistance = 10.0f;
	static constexpr float kBossPostMeleeEvadeSpeed = 32.0f;

	// 보스 스테이지는 중심 (0, 0, 400), 안전 판정은 210 x 210 사용.
	static constexpr float kBossPostMeleeEvadeStageCenterX = 0.0f;
	static constexpr float kBossPostMeleeEvadeStageCenterZ = 400.0f;
	static constexpr float kBossPostMeleeEvadeStageHalfExtent = 105.0f;

	// 보스 크기/벽 끼임 방지용 여유.
	static constexpr float kBossPostMeleeEvadeStagePadding = 5.0f;

	bool m_bBossPostMeleeEvading = false;

	XMFLOAT3 m_bossMeleeAttackForward =
		XMFLOAT3(0.0f, 0.0f, 1.0f);

	XMFLOAT3 m_bossPostMeleeEvadeDirection =
		XMFLOAT3(0.0f, 0.0f, 0.0f);

	XMFLOAT3 m_bossPostMeleeEvadeTarget =
		XMFLOAT3(0.0f, 0.0f, 0.0f);

	float m_bossPostMeleeEvadeRemainingDistance = 0.0f;

	// 회피 이동 중 플레이어를 바라보는 회전 속도.
	float m_bossPostMeleeTurnSpeedDegrees = 900.0f;

	float m_bossSpellTurnSpeedDegrees = 720.0f;
};