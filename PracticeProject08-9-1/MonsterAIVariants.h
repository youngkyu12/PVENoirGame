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
		Spell
	};

private:
	void UpdateBossCooldowns(float dt);
	void ConfigureBossHitReactionPolicy();

	bool IsPlayerInsideBossBattleZone(CGameObject* player) const;
	bool CanStartBossAction() const;

	bool TryPerformBossCommand(EMonsterAnimCommand command);
	bool TryPerformMeleeAttack();
	bool TryPerformSpellAttack();

	void ConsumeBossMeleeCooldown();
	void ConsumeBossSpellCooldown();

	bool IsBossMeleeActionPlaying() const;
	bool IsBossSpellActionPlaying() const;

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